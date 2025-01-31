﻿#pragma once

#include "ofMain.h"
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

	ofEasyCam _camera;
	std::shared_ptr<houdini_alembic::AlembicScene> _alembicscene;
	houdini_alembic::PolygonMeshObject *_mesh = nullptr;
	std::vector<uint32_t> _indices;
	std::vector<glm::vec3> _points;
	std::vector<glm::vec3> _normals;
};
