#include "ofApp.h"

#include <stdexcept>
#include <vector>

using namespace std;
using namespace cv;
using namespace ofxCv;

//--------------------------------------------------------------
ofApp::ofApp()
: ofBaseApp()
, vidGrabber()
, finders()
, currentFinder(-1)
, framesLost(ofApp::FRAMES_LOST_LIMIT)
, kalman()
, tracks()
, peaks()
, estimatedBeatDurations()
, estimatedBeatPhases()
, beatKalman()
, beatDuration(0.f)
, beatPhase(0.f)
, font()
, metronome(false)
, debug(true)
{
}

//--------------------------------------------------------------
void ofApp::setup()
{
    // Setup video grabbing
    vector<ofVideoDevice> devices = vidGrabber.listDevices();
    unsigned int iVideoDevice = 0;
    while (iVideoDevice < devices.size() && !(devices[iVideoDevice].bAvailable))
    {
        iVideoDevice++;
    }
    if (iVideoDevice >= devices.size())
    {
        throw runtime_error("No available video capture device found.");
    }

    ofLogNotice() << "Caputring video from device " << devices[iVideoDevice].id
                  << ": " << devices[iVideoDevice].deviceName;
    vidGrabber.setDeviceID(0);
    vidGrabber.setDesiredFrameRate(60);
    vidGrabber.initGrabber(CAMERA_WIDTH, CAMERA_HEIGHT);

    // Setup finders
    addObjectFinder("haarcascade_frontalface_default.xml");
    addObjectFinder("haarcascade_profileface.xml");
    currentFinder = -1;
    framesLost = FRAMES_LOST_LIMIT;

    // Setup Kalman filter
    kalman.init(1.e0f, 10.e0f, false); // inverse of (smoothness, rapidness)

    // Setup Kalman filter for beat tracking (abusing geometric concepts!)
    beatKalman.init(1.e-6f, 1.e-3f, false);
    beatDuration = 0.f;
    beatPhase = 0.f;

    // Setup window
    ofSetWindowTitle("Bobblehead");
    ofSetWindowShape(vidGrabber.getWidth(), vidGrabber.getHeight());
    ofSetWindowPosition((ofGetScreenWidth() - vidGrabber.getWidth()) / 2,
                        (ofGetScreenHeight() - vidGrabber.getHeight()) / 2);
    ofSetVerticalSync(true);
    ofBackground(100, 100, 100);

    // Setup font
    font.load("verdana.ttf", 30, true, true);
    font.setLineHeight(34.0f);
    font.setLetterSpacing(1.035);
}

//--------------------------------------------------------------
void ofApp::addObjectFinder(const string &cascadeFilename)
{
    ObjectFinder finder;
    finder.setup(cascadeFilename);
    finder.setRescale(.25);
    finder.setFindBiggestObject(true);
    finder.getTracker().setSmoothingRate(.3);
    finders.push_back(move(finder));
}

//--------------------------------------------------------------
void ofApp::update()
{
    // Erase tracks out of time window
    const auto timestamp = ofGetElapsedTimef();
    while (!tracks.empty() &&
           (timestamp - tracks.front().timestamp) > BEAT_TRACK_WINDOW)
    {
        tracks.pop_front();
    }

    vidGrabber.update();
    if (vidGrabber.isFrameNew())
    {
        findObject();
        estimateBeat();
    }
}

//--------------------------------------------------------------
void ofApp::findObject()
{
    const auto timestamp = ofGetElapsedTimef();
    if (framesLost < FRAMES_LOST_LIMIT)
    {
        framesLost++;
    }

    // Try current finder
    bool trackingUpdated{false};
    if (currentFinder >= 0 && currentFinder < int(finders.size()))
    {
        finders[currentFinder].update(vidGrabber);
        if (finders[currentFinder].size() > 0)
        {
            kalman.update(
                finders[currentFinder].getObjectSmoothed(0).getCenter());
            trackingUpdated = true;
        }
    }

    // If object has been lost try every other finder
    if (!trackingUpdated && framesLost >= FRAMES_LOST_LIMIT)
    {
        tracks.clear();
        ofVec2f bestPoint;
        float bestArea{0.f};
        int bestIdx{-1};
        for (unsigned int i = 0; i < finders.size(); i++) {
            if (int(i) == currentFinder)
            {
                continue;
            }
            finders[i].update(vidGrabber);
            if (finders[i].size() > 0)
            {
                auto obj = finders[i].getObjectSmoothed(0);
                auto objArea = obj.getArea();
                if (objArea > bestArea)
                {
                    bestPoint = obj.getCenter();
                    bestArea = objArea;
                    bestIdx = i;
                }
            }
        }

        if (bestIdx >= 0)
        {
            currentFinder = bestIdx;
            kalman.update(bestPoint);
            trackingUpdated = true;
        }
    }

    if (trackingUpdated)
    {
        framesLost = 0;
        // Add new tracked point
        tracks.push_back(
            {timestamp, kalman.getEstimation(), kalman.getVelocity()});
    }
}

//--------------------------------------------------------------
void ofApp::estimateBeat()
{
    static constexpr float MIN_PREV_PEAK_VELOCITY{1.f};
    static constexpr float MAX_PEAK_VELOCITY{5.f};

    if (tracks.size() < 3)
    {
        return;
    }

    // Find peaks in distance from average
    peaks.clear();
    auto it = tracks.begin();
    auto velPrev = it->velocity;
    auto velLenPrev = velPrev.length();
    ++it;
    auto vel = it->velocity;
    auto velLen = vel.length();
    auto timestamp = it->timestamp;
    ++it;
    for (; it != tracks.end(); ++it) {
        auto velNext = it->velocity;
        auto velLenNext = velNext.length();
        // Check within bounds
        if (velLenPrev >= MIN_PREV_PEAK_VELOCITY && velLen <= MAX_PEAK_VELOCITY)
        {
            if (!vel.align(velPrev, 60) ||
                ((velLenPrev > velLen * 1.2f) && (velLen <= velLenNext)))
            {
                peaks.push_back(timestamp);
            }
        }
        // Move forward
        velPrev = vel;
        velLenPrev = velLen;
        vel = velNext;
        velLen = velLenNext;
        timestamp = it->timestamp;
    }
    if (peaks.empty())
    {
        return;
    }

    // Estimate beat discarding out of limit values
    estimatedBeatDurations.clear();
    estimatedBeatPhases.clear();
    auto lastBeat = peaks.front();
    for (auto timestamp : peaks) {
        auto timeDiff = timestamp - lastBeat;
        if (timeDiff >= (60.f / MAX_TEMPO))
        {
            lastBeat = timestamp;
            if (timeDiff <= (60.f / MIN_TEMPO))
            {
                estimatedBeatDurations.push_back(timeDiff);
                auto phase = timestamp - floor(timestamp / timeDiff) * timeDiff;
                estimatedBeatPhases.push_back(phase);
            }
        }
    }
    auto numEstimations = estimatedBeatDurations.size();
    if (numEstimations > 0)
    {
        // Median beat estimation
        float estimatedDuration;
        float estimatedPhase;
        sort(estimatedBeatDurations.begin(), estimatedBeatDurations.end());
        sort(estimatedBeatPhases.begin(), estimatedBeatPhases.end());
        if (numEstimations % 2 == 0)
        {
            estimatedDuration =
                (estimatedBeatDurations[numEstimations / 2] +
                 estimatedBeatDurations[(numEstimations / 2) + 1]) /
                2.f;
            estimatedPhase = (estimatedBeatPhases[numEstimations / 2] +
                              estimatedBeatPhases[(numEstimations / 2) + 1]) /
                             2.f;
        }
        else
        {
            estimatedDuration = estimatedBeatDurations[numEstimations / 2];
            estimatedPhase = estimatedBeatPhases[numEstimations / 2];
        }
        beatKalman.update(ofVec2f(estimatedDuration, estimatedPhase));
        auto newBeatEstimation = beatKalman.getEstimation();
        beatDuration = newBeatEstimation.x;
        beatPhase = newBeatEstimation.y;
    }
}

//--------------------------------------------------------------
void ofApp::draw()
{
    ofSetHexColor(0xffffff);
    vidGrabber.draw(0, 0);

    if (debug)
    {
        if (currentFinder >= 0 && currentFinder <= int(finders.size()))
        {
            finders[currentFinder].draw();
        }

        // Draw tracked points
        for (const auto &track : tracks) {
            int h = int(round(track.timestamp * 100.f)) % 256;
            ofSetColor(ofColor::fromHsb(h, 255, 255));
            ofDrawCircle(track.point.x, track.point.y, 3.f);
            ofSetLineWidth(2.f);
            ofDrawLine(track.point, track.point + 3.f * track.velocity);
        }
    }

    unsigned int tempo = beatDuration > 0.f ? round(60.f / beatDuration) : 0;
    if (tempo >= MIN_TEMPO && tempo <= MAX_TEMPO)
    {
        ofSetHexColor(0xffffff);
        font.drawString("Tempo: " + ofToString(tempo) + " bpm", 20, 40);

        if (metronome)
        {
            const auto timestamp = ofGetElapsedTimef();
            float lastBeat =
                beatDuration * floor((timestamp - beatPhase) / beatDuration) +
                beatPhase;
            // ofLog() << "timestamp: " << timestamp;
            // ofLog() << "lastBeat: " << lastBeat;
            // ofLog() << "beatDuration: " << beatDuration;
            // ofLog() << "beatPhase: " << beatPhase;
            if ((timestamp - lastBeat) < .5f * beatDuration)
            {
                ofSetHexColor(0xff0000);
                ofDrawCircle(40.f, 80.f, 20.f);
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    switch (key)
    {
    case 'd':
        debug = !debug;
        break;
    case 'm':
        metronome = !metronome;
        break;
    default:
        ;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    ofSetWindowShape(vidGrabber.getWidth(), vidGrabber.getHeight());
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg)
{
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{
}
