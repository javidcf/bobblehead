#pragma once

#include "ofMain.h"
#include "ofxCv.h"

class ofApp : public ofBaseApp
{

public:
    ofApp();

    void setup();
    void update();
    void draw();

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);

private:
    static constexpr float BEAT_TRACK_WINDOW = 5.f;

    // Try to grab camera at this size
    static constexpr unsigned int CAMERA_WIDTH = 640;
    static constexpr unsigned int CAMERA_HEIGHT = 480;

    // Tempo limits
    static constexpr unsigned int MIN_TEMPO = 60;
    static constexpr unsigned int MAX_TEMPO = 240;

    // Frames to consider an object has been lost
    static constexpr unsigned int FRAMES_LOST_LIMIT = 15;

    struct TimedTrack
    {
        float timestamp;
        ofVec2f point;
        ofVec2f velocity;
    };

    ofVideoGrabber vidGrabber;
    vector<ofxCv::ObjectFinder> finders;
    int currentFinder;
    unsigned int framesLost;
    ofxCv::KalmanPosition kalman;
    deque<TimedTrack> tracks;
    vector<float> peaks;
    vector<float> estimatedBeatDurations;
    vector<float> estimatedBeatPhases;
    ofxCv::KalmanPosition beatKalman;
    float beatDuration;
    float beatPhase;
    ofTrueTypeFont font;
    bool metronome;
    bool debug;

    void addObjectFinder(const string &cascadeFilename);
    void findObject();
    void estimateBeat();
};
