// -*- C++ -*-
//
// Package:    TrackAssociator
// Class:      TestTrackAssociatorProducer
// 
/*

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Dmytro Kovalskyi
//         Created:  Fri Apr 21 10:59:41 PDT 2006
// $Id: TestTrackAssociatorProducer.cc,v 1.5 2006/10/09 18:24:33 jribnik Exp $
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/OrphanHandle.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackExtra.h"
#include "DataFormats/CaloTowers/interface/CaloTower.h"
#include "DataFormats/CaloTowers/interface/CaloTowerCollection.h"
#include "DataFormats/EgammaReco/interface/SuperCluster.h"
#include "DataFormats/DTRecHit/interface/DTRecHitCollection.h"
#include "DataFormats/EcalDetId/interface/EBDetId.h"

#include "SimDataFormats/TrackingHit/interface/PSimHit.h"
#include "SimDataFormats/TrackingHit/interface/PSimHitContainer.h"
#include "SimDataFormats/Track/interface/SimTrack.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/Vertex/interface/SimVertex.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "SimDataFormats/CaloHit/interface/PCaloHit.h"
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"

// calorimeter info
#include "Geometry/Records/interface/IdealGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "DataFormats/EcalDetId/interface/EcalSubdetector.h"
#include "DataFormats/HcalDetId/interface/HcalSubdetector.h"

#include "Geometry/DTGeometry/interface/DTLayer.h"
#include "Geometry/DTGeometry/interface/DTGeometry.h"
#include "Geometry/Records/interface/MuonGeometryRecord.h"

#include "Geometry/Surface/interface/Cylinder.h"
#include "Geometry/Surface/interface/Plane.h"

#include "Geometry/CommonDetUnit/interface/GeomDetUnit.h"


#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

#include "TrackPropagation/SteppingHelixPropagator/interface/SteppingHelixPropagator.h"

#include <boost/regex.hpp>

#include "TrackingTools/TrackAssociator/interface/TrackAssociator.h"
#include "TrackingTools/TrackAssociator/interface/TimerStack.h"
#include "TrackingTools/TrackAssociator/interface/TrackDetMatchInfoCollection.h"

#include "CLHEP/HepPDT/ParticleID.hh"

class TestTrackAssociatorProducer : public edm::EDProducer {
  public:
    explicit TestTrackAssociatorProducer(const edm::ParameterSet&);
    virtual ~TestTrackAssociatorProducer(){};

    virtual void produce (edm::Event&, const edm::EventSetup&);

  private:
    TrackAssociator trackAssociator_;
    bool useSimTrack_;
    bool useEcal_;
    bool useHcal_;
    bool useMuon_;
    std::string recoTrackCollectionName_;
};

TestTrackAssociatorProducer::TestTrackAssociatorProducer(const edm::ParameterSet& iConfig)
{
  recoTrackCollectionName_ = iConfig.getParameter<std::string>("recoTrackCollectionName");

  produces<TrackDetMatchInfoCollection>("SimTracks");
  produces<TrackDetMatchInfoCollection>(recoTrackCollectionName_);

  useSimTrack_ = iConfig.getParameter<bool>("useSimTrack");
  useEcal_ = iConfig.getParameter<bool>("useEcal");
  useHcal_ = iConfig.getParameter<bool>("useHcal");
  useMuon_ = iConfig.getParameter<bool>("useMuon");


  // Fill data labels
  std::vector<std::string> labels = iConfig.getParameter<std::vector<std::string> >("labels");
  boost::regex regExp1 ("([^\\s,]+)[\\s,]+([^\\s,]+)$");
  boost::regex regExp2 ("([^\\s,]+)[\\s,]+([^\\s,]+)[\\s,]+([^\\s,]+)$");
  boost::smatch matches;

  for(std::vector<std::string>::const_iterator label = labels.begin(); label != labels.end(); label++) {
    if (boost::regex_match(*label,matches,regExp1))
      trackAssociator_.addDataLabels(matches[1],matches[2]);
    else if (boost::regex_match(*label,matches,regExp2))
      trackAssociator_.addDataLabels(matches[1],matches[2],matches[3]);
    else
      edm::LogError("ConfigurationError") << "Failed to parse label:\n" << *label << "Skipped.\n";
  }

  // trackAssociator_.addDataLabels("EBRecHitCollection","ecalrechit","EcalRecHitsEB");
  // trackAssociator_.addDataLabels("CaloTowerCollection","towermaker");
  // trackAssociator_.addDataLabels("DTRecSegment4DCollection","recseg4dbuilder");


  trackAssociator_.useDefaultPropagator();
}

void TestTrackAssociatorProducer::produce( edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;
  using namespace reco;

  std::auto_ptr<TrackDetMatchInfoCollection> simTrackOutput(new TrackDetMatchInfoCollection());
  std::auto_ptr<TrackDetMatchInfoCollection> recTrackOutput(new TrackDetMatchInfoCollection());

  // use simulated tracks?
  if(useSimTrack_) {

    // get list of tracks and their vertices
    Handle<SimTrackContainer> simTracks;
    iEvent.getByType<SimTrackContainer>(simTracks);
    if (! simTracks.isValid() ) throw cms::Exception("FatalError") << "No SimTracks found\n";

    Handle<SimVertexContainer> simVertices;
    iEvent.getByType<SimVertexContainer>(simVertices);
    if (! simVertices.isValid() ) throw cms::Exception("FatalError") << "No SimVertexs found\n";

    // loop over simulated tracks
    std::cout << "Number of simulated tracks found in the event: " << simTracks->size() << std::endl;
    SimTrackContainer::size_type itemIndex = 0;
    for(SimTrackContainer::const_iterator tracksCI = simTracks->begin(); tracksCI != simTracks->end(); ++tracksCI) {

      SimTrackRef simTrackRef(simTracks, itemIndex);
      itemIndex++;

      // only propagate charged tracks!
      HepPDT::ParticleID id(tracksCI->type());
      if(!id.threeCharge()) continue;

      // skip low Pt tracks
      if (tracksCI->momentum().perp() < 2.) {
        std::cout << "Skipped low Pt track (Pt: " << tracksCI->momentum().perp() << ")" <<std::endl;
        continue;
      }

      // get vertex
      int vertexIndex = tracksCI->vertIndex();

      SimVertex vertex(Hep3Vector(0.,0.,0.),0);
      if (vertexIndex >= 0) vertex = (*simVertices)[vertexIndex];

      // skip tracks originated away from the IP (in mm)
      if(!(fabs(vertex.position().z()) < 300 && vertex.position().perp() < 30)) {
        std::cout << "Skipped track originated away from IP: z=" << vertex.position().z() << " perp=" << vertex.position().perp() <<std::endl;
        continue;
      }

      std::cout << "\n-------------------------------------------------------\n Track (pt,eta,phi): " << tracksCI->momentum().perp() << " , " << tracksCI->momentum().eta() << " , " << tracksCI->momentum().phi() << std::endl;

      TrackAssociator::AssociatorParameters parameters;
      parameters.useEcal = useEcal_ ;
      parameters.useHcal = useHcal_ ;
      parameters.useMuon = useMuon_ ;
      parameters.dREcal = 0.03;
      parameters.dRHcal = 0.07;
      parameters.dRMuon = 0.3;

      TrackDetMatchInfo info = trackAssociator_.associate(iEvent, iSetup,
          trackAssociator_.getFreeTrajectoryState(iSetup, *tracksCI, vertex),
          parameters);

      info.simTrackRef_ = simTrackRef; 

      simTrackOutput->push_back(info);
    }//for
  }//if

  // get list of reconstructed tracks
  Handle<TrackCollection> recoTracks;
  iEvent.getByLabel(recoTrackCollectionName_, recoTracks);
  if(!recoTracks.isValid()) throw cms::Exception("FatalError") << "No " << recoTrackCollectionName_ << " found\n";

  // loop over reconstructed tracks
  std::cout << "Number of reconstructed tracks found in the event: " << recoTracks->size() << std::endl;
  TrackCollection::size_type itemIndex = 0;
  for(TrackCollection::const_iterator tracksCI = recoTracks->begin(); tracksCI != recoTracks->end(); ++tracksCI) {

    TrackRef trackRef(recoTracks, itemIndex);
    itemIndex++;

    // skip low Pt tracks
    if(tracksCI->pt() < 2.) {
      std::cout << "Skipped low Pt track (Pt: " << tracksCI->pt() << ")" <<std::endl;
      continue;
    }

    // skip tracks originated away from the IP (in cm)
    if(!(fabs(tracksCI->vertex().z()) < 30 && tracksCI->vertex().rho() < 3)) {//this rho is sqrt(perp2())
      std::cout << "Skipped track originated away from IP: " <<tracksCI->vertex().rho()<<std::endl;
      continue;
    }

    std::cout << "\n-------------------------------------------------------\n Track (pt,eta,phi): " << tracksCI->pt() << " , " << tracksCI->eta() << " , " << tracksCI->phi() << std::endl;

    // Get HCAL energy in more generic way
    TrackAssociator::AssociatorParameters parameters;
    parameters.useEcal = useEcal_ ;
    parameters.useHcal = useHcal_ ;
    parameters.useMuon = useMuon_ ;
    parameters.dREcal = 0.03;
    parameters.dRHcal = 0.07;
    parameters.dRMuon = 0.3;

    TrackDetMatchInfo info = trackAssociator_.associate(iEvent, iSetup,
        trackAssociator_.getFreeTrajectoryState(iSetup, *tracksCI),
        parameters);

    info.trackRef_ = trackRef;

    recTrackOutput->push_back(info);
  }//for

  iEvent.put(simTrackOutput, "SimTracks");
  iEvent.put(recTrackOutput, recoTrackCollectionName_);
} 
//define this as a plug-in
DEFINE_FWK_MODULE(TestTrackAssociatorProducer);
