﻿#pragma once

#include "ofMain.h"

#include "ofxOsc.h"
#include "houdini_alembic.hpp"
#include "alembic_preview.hpp"

class ofApp : public ofBaseApp{
public:
	void setup();
	void exit();

	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	void initialize_render();

	ofEasyCam _camera;
	ofImage _image;

	ofMesh _camera_model;
	std::shared_ptr<houdini_alembic::AlembicScene> _alembicscene;

	ofxOscReceiver _osc;
};
