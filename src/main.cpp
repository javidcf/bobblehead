#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
    // Setup the GL context
    ofSetupOpenGL(800, 600, OF_WINDOW);

    // This kicks off the running of my app
    // Can be OF_WINDOW or OF_FULLSCREEN
    // Pass in width and height too
    ofRunApp(new ofApp());

}
