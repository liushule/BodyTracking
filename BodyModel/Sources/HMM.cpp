#include "pch.h"
#include "HMM.h"

#include <Kore/Log.h>

#include <algorithm>
#include <iterator>
#include <valarray>

namespace {
	const char hmmPath[] = "../../HMM_Trainer/Tracking/";
	const char hmmPath0[] = "../../HMM_Trainer/Tracking/Movement0/";
	const char hmmPath1[] = "../../HMM_Trainer/Tracking/Movement1/";
	const char hmmPath2[] = "../../HMM_Trainer/Tracking/Movement2/";
	const char hmmName[] = "Yoga_Krieger";
	
	double startX;
	double startZ;
	double startRotCos;
	double startRotSin;
	double transitionX;
	double transitionY;
	double transitionZ;
	float currentUserHeight;
	
	int curentFileNumber = 0;
	int curentLineNumber = 0;
	
	const int numOfDataPoints = 6;
	int dataPointNumber; // x-th point of data collected in current recording/recognition
	
	// Vector of data points logged in real time movement recognition
	std::vector<std::vector<Point>> recognitionPoints(numOfDataPoints);
	
}

HMM::HMM(Logger& logger) : logger(logger), recording(false), recognizing(false) {
	curentFileNumber = 0;
}

bool HMM::isRecordingActive() {
	// Recording is only possible while there is no recognition in progress
	return (record && !recognition);
}

bool HMM::isRecognitionActive() {
	// Recognition is only possible while there is no recording in progress
	return (recognition && !record);
}

void HMM::init(Kore::vec3 hmdPosition, Kore::Quaternion hmdRotation) {
	currentUserHeight = hmdPosition.y();
	
	curentLineNumber = 0;
}

void HMM::startRecording(Kore::vec3 hmdPosition, Kore::Quaternion hmdRotation) {
	if (record) {
		init(hmdPosition, hmdRotation);
		logger.startHMMLogger(hmmName, curentFileNumber);
		curentFileNumber++;
	}
}

void HMM::stopRecording() {
	if (record) {
		logger.endHMMLogger();
	}
}

void HMM::startRecognition(Kore::vec3 hmdPosition, Kore::Quaternion hmdRotation) {
	if (recognition) {
		// Clear prievously stored points
		for (int i = 0; i < numOfDataPoints; i++) recognitionPoints.at(i).clear();
		dataPointNumber = 0;
		
		init(hmdPosition, hmdRotation);
	}
}

bool HMM::stopRecognition() {
	if (recognition) {
		// Read clusters for all trackers from file
		bool trackersPresent[numOfDataPoints];
		vector<KMeans> kmeanVector(numOfDataPoints);
		for (int ii = 0; ii < numOfDataPoints; ii++) {
			try {
				char hmmNameWithNum[50];
				sprintf(hmmNameWithNum, "%s_%i", hmmName, ii);
				KMeans kmeans(hmmPath, hmmNameWithNum);
				kmeanVector.at(ii) = kmeans;
				trackersPresent[ii] = true;
			} catch (std::invalid_argument) {
				trackersPresent[ii] = false;
				Kore::log(Kore::Error, "Can't find tracker file");
			}
		}
		// Store which trackers were recognised as the correct
		vector<bool> trackerMovementRecognised(numOfDataPoints, true);
		for (int ii = 0; ii < numOfDataPoints; ii++) {
			// Make sure the tracker is currently present and there is a HMM for it
			if (!recognitionPoints.at(ii).empty() && trackersPresent[ii]) {
				// Clustering data
				vector<int> clusteredPoints = kmeanVector.at(ii).matchPointsToClusters(normaliseMeasurements(recognitionPoints.at(ii), kmeanVector.at(ii).getAveragePoints()));
				// Reading HMM
				char hmmNameWithNum[50];
				sprintf(hmmNameWithNum, "%s_%i", hmmName, ii);
				HMMModel model(hmmPath, hmmNameWithNum);
				// Calculating the probability and comparing with probability threshold as well as applying restfix
				float n = 0;
				n = model.calculateProbability(clusteredPoints);
				Kore::log(Kore::LogLevel::Info, "Probability: (%f,%f)", n, model.getProbabilityThreshold());
				//trackerMovementRecognised.at(ii) = (model.calculateProbability(clusteredPoints) > model.getProbabilityThreshold() && !std::equal(clusteredPoints.begin() + 1, clusteredPoints.end(), clusteredPoints.begin()));
				trackerMovementRecognised.at(ii) = (model.calculateProbability(clusteredPoints) > model.getProbabilityThreshold());
				logger.analyseHMM(hmmName, model.calculateProbability(clusteredPoints), false);
			}
		}
		
		logger.analyseHMM(hmmName, 0, true);
//        int truenumber = 0;
//        for(int i=0;i<numOfDataPoints;i++) {
//            if (trackerMovementRecognised.at(i) == true) {
//                truenumber++;
//            }
//        }
//                Kore::log(Kore::LogLevel::Info, "truenumber: (%d)", truenumber);
//
		
		if (std::all_of(trackerMovementRecognised.begin(), trackerMovementRecognised.end(), [](bool v) { return v; })) {
//            if (truenumber > 5) {
			// All (present) trackers were recognised as correct
			return true;
		}
        else {return false;}
	}
	return false;
}

bool HMM::stopRecognitionAndIdentify() {
	if (recognition) {
		// Read clusters for all trackers from file
		bool trackersPresent[numOfDataPoints];
		vector<vector<KMeans>> kmeanVector(3, vector <KMeans> (numOfDataPoints));
		for (int ii = 0; ii < numOfDataPoints; ii++) {
			try {
				char hmmNameWithNum[50];
				sprintf(hmmNameWithNum, "%s_%i", hmmName, ii);
				KMeans kmeans0(hmmPath0, hmmNameWithNum);
                KMeans kmeans1(hmmPath1, hmmNameWithNum);
                KMeans kmeans2(hmmPath2, hmmNameWithNum);
				kmeanVector.at(0).at(ii) = kmeans0;
                kmeanVector.at(1).at(ii) = kmeans1;
                kmeanVector.at(2).at(ii) = kmeans2;
				trackersPresent[ii] = true;
			} catch (std::invalid_argument) {
				trackersPresent[ii] = false;
				Kore::log(Kore::Error, "Can't find tracker file");
			}
		}
		// Store which trackers were recognised as the correct
		vector<bool> trackerMovementRecognised(numOfDataPoints, true);
		vector<vector<int>>clusteredPoints(3, vector <int>());
        vector<int>identifyProbability;
        vector<double>probabilityModel0;
        vector<double>probabilityModel1;
        vector<double>probabilityModel2;
        vector<double>probabilityStandardModel0;
        vector<double>probabilityStandardModel1;
        vector<double>probabilityStandardModel2;
        double p0,p1,p2,ps0,ps1,ps2;
		// Create 3 HHModel for 3 different movement
		for (int ii = 0; ii < numOfDataPoints; ii++) {
			// Make sure the tracker is currently present and there is a HMM for it
			if (!recognitionPoints.at(ii).empty() && trackersPresent[ii]) {
				// Clustering data
				clusteredPoints.at(0) = kmeanVector.at(0).at(ii).matchPointsToClusters(normaliseMeasurements(recognitionPoints.at(ii), kmeanVector.at(0).at(ii).getAveragePoints()));
                clusteredPoints.at(1) = kmeanVector.at(1).at(ii).matchPointsToClusters(normaliseMeasurements(recognitionPoints.at(ii), kmeanVector.at(1).at(ii).getAveragePoints()));
                clusteredPoints.at(2) = kmeanVector.at(2).at(ii).matchPointsToClusters(normaliseMeasurements(recognitionPoints.at(ii), kmeanVector.at(2).at(ii).getAveragePoints()));
				// Reading HMM
				char hmmNameWithNum[50];
				sprintf(hmmNameWithNum, "%s_%i", hmmName, ii);
				HMMModel model0(hmmPath0, hmmNameWithNum);
				HMMModel model1(hmmPath1, hmmNameWithNum);
				HMMModel model2(hmmPath2, hmmNameWithNum);
                p0 = model0.calculateProbability(clusteredPoints.at(0));
                p1 = model1.calculateProbability(clusteredPoints.at(1));
                p2 = model2.calculateProbability(clusteredPoints.at(2));
                ps0 = model0.getProbabilityThreshold();
                ps1 = model1.getProbabilityThreshold();
                ps2 = model2.getProbabilityThreshold();
				vector<double> probabilityCurrentmovement = {p0,p1,p2};
                probabilityModel0.push_back(p0);
                probabilityModel1.push_back(p1);
                probabilityModel2.push_back(p2);
                probabilityStandardModel0.push_back(ps0);
                probabilityStandardModel1.push_back(ps1);
                probabilityStandardModel2.push_back(ps2);
				Kore::log(Kore::LogLevel::Info, "model0: (%f,%f)", model0.calculateProbability(clusteredPoints.at(0)), model0.getProbabilityThreshold());
				Kore::log(Kore::LogLevel::Info, "model1: (%f,%f)", model1.calculateProbability(clusteredPoints.at(1)), model1.getProbabilityThreshold());
				Kore::log(Kore::LogLevel::Info, "model2: (%f,%f)", model2.calculateProbability(clusteredPoints.at(2)), model2.getProbabilityThreshold());
                
                std::vector<double>::iterator biggest = std::max_element(std::begin(probabilityCurrentmovement), std::end(probabilityCurrentmovement));
			//	Kore::log(Kore::LogLevel::Info, "max: (%f)", probabilityCurrentmovement.end());
				identifyProbability.push_back(std::distance(std::begin(probabilityCurrentmovement), biggest));
			}
		}
        int n0,n1,n2 = -1;
        n0= count(identifyProbability.begin(),identifyProbability.end(),0);
        n1= count(identifyProbability.begin(),identifyProbability.end(),1);
        n2= count(identifyProbability.begin(),identifyProbability.end(),2);
		logger.analyseHMM(hmmName, 0, true);
		Kore::log(Kore::LogLevel::Info, "Probability: (%d,%d,%d)", n0,n1,n2);
        if(n0 > n1 && n0 > n2){
            log(Kore::Info, "Current movement is recognized as Pose 1.");
            for (int ii = 0; ii < numOfDataPoints; ii++) {
            trackerMovementRecognised.at(ii) = ( probabilityModel0.at(ii)>probabilityStandardModel0.at(ii) );
              }
        }
        if(n1 > n0 && n1> n2){
            log(Kore::Info, "Current movement is recognized as Pose 2.");
            for (int ii = 0; ii < numOfDataPoints; ii++) {
            trackerMovementRecognised.at(ii) = ( probabilityModel1.at(ii)>probabilityStandardModel1.at(ii) );
           }
        }
        else if(n2 > n0 && n2> n1)
        {
            log(Kore::Info, "Current movement is recognized as Pose 3.");
            for (int ii = 0; ii < numOfDataPoints; ii++) {
                trackerMovementRecognised.at(ii) = ( probabilityModel1.at(ii)>probabilityStandardModel1.at(ii) );
            }
            
        }
        else{log(Kore::Info, "None movement is recognized");}
        if (std::all_of(trackerMovementRecognised.begin(), trackerMovementRecognised.end(), [](bool v) { return v; })) {
			// All (present) trackers were recognised as correct
			return true;}
        else {
			return false;
        }}
	return false;
}

bool HMM::hmmActive() {
	if (recording || recognizing) return true;
	else return false;
}

bool HMM::hmmRecording() {
	return recording;
}

bool HMM::hmmRecognizing() {
	return recognizing;
}

void HMM::recordMovement(float lastTime, const char* name, Kore::vec3 position, Kore::Quaternion rotation) {
	// Either recording or recognition is active
	if (recording || recognizing) {
		
		curentLineNumber++;

		transitionX = position.x() - startX;
		transitionY = position.y();
		transitionZ = position.z() - startZ;
		if (record) {
			// Data is recorded
			logger.saveHMMData(name, lastTime, position.normalize(), rotation);
		}
		
		if (recognition) { // data is stored internally for evaluation at the end of recognition
			double x, y, z, rotw, rotx, roty, rotz;
			// TODO: do we need to normalize position?
			x = position.normalize().x();
			y = position.normalize().y();
			z = position.normalize().z();
			rotw = rotation.w;
			rotx = rotation.x;
			roty = rotation.y;
			rotz = rotation.z;
//			x = (transitionX * startRotCos - transitionZ * startRotSin);
//			y = (transitionY / currentUserHeight) * 1.8;
//			z = (transitionZ * startRotCos + transitionX * startRotSin);
			
			vector<double> values = { x, y, z, rotx, roty, rotz, rotw };
			Point point = Point(dataPointNumber, values);
			dataPointNumber++;
			if (std::strcmp(name, headTag) == 0)		recognitionPoints.at(0).push_back(point);
			else if (std::strcmp(name, lHandTag) == 0)	recognitionPoints.at(1).push_back(point);
			else if (std::strcmp(name, rHandTag) == 0)	recognitionPoints.at(2).push_back(point);
			else if (std::strcmp(name, hipTag) == 0)	recognitionPoints.at(3).push_back(point);
			else if (std::strcmp(name, lFootTag) == 0)	recognitionPoints.at(4).push_back(point);
			else if (std::strcmp(name, rFootTag) == 0)	recognitionPoints.at(5).push_back(point);
			
			
		}
	}
}
