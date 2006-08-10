// -*- C++ -*-
//
// Package:    TrackAssociator
// Class:      TestMuonProducer
// 
/*

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Dmytro Kovalskyi
//         Created:  Fri Apr 21 10:59:41 PDT 2006
// $Id: MuonTest.cc,v 1.1 2006/06/09 17:30:23 dmytro Exp $
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

#include "TrackingTools/TrackAssociator/interface/TrackAssociator.h"

#include "TrackingTools/TrackAssociator/interface/TestMuon.h"

#include "TrackingTools/TrackAssociator/interface/TimerStack.h"

#include <boost/regex.hpp>

class TestMuonProducer : public edm::EDProducer {
 public:
   explicit TestMuonProducer(const edm::ParameterSet&);
   
   ~TestMuonProducer();
   
   virtual void produce(edm::Event&, const edm::EventSetup&);

 private:
   TrackAssociator trackAssociator_;
   bool useEcal_;
   bool useHcal_;
   bool useMuon_;
};

TestMuonProducer::TestMuonProducer(const edm::ParameterSet& iConfig)
{
   produces<reco::TestMuonCollection>("muons");

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
   
   trackAssociator_.useDefaultPropagator();
}


TestMuonProducer::~TestMuonProducer()
{
  TimingReport::current()->dump(std::cout);
}

void TestMuonProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
   using namespace edm;
   
   std::auto_ptr<reco::TestMuonCollection> outputMuons(new reco::TestMuonCollection);

   TimerStack timers;
   timers.push("TestMuonProducer::produce::init");
   
   // get list of tracks and their vertices
   timers.pop_and_push("TestMuonProducer::produce::init::simTracks");
   Handle<SimTrackContainer> simTracks;
   iEvent.getByType<SimTrackContainer>(simTracks);
   
   timers.pop_and_push("TestMuonProducer::produce::init::simVertices");
   Handle<SimVertexContainer> simVertices;
   iEvent.getByType<SimVertexContainer>(simVertices);
   if (! simVertices.isValid() ) throw cms::Exception("FatalError") << "No tracks found\n";
   
   timers.clean_stack();
   
   // loop over simulated tracks
   for(SimTrackContainer::const_iterator tracksCI = simTracks->begin(); 
       tracksCI != simTracks->end(); tracksCI++){
      // get vector
      Hep3Vector trackP3 = tracksCI->momentum().vect();

      // skip low Pt tracks
      if (tracksCI->momentum().perp() < 5) continue;
      
      // get vertex
      int vertexIndex = tracksCI->vertIndex();
      // uint trackIndex = tracksCI->genpartIndex();
      
      SimVertex vertex(Hep3Vector(0.,0.,0.),0);
      if (vertexIndex >= 0) vertex = (*simVertices)[vertexIndex];
      
      // Ignore vertex. The track can only be initialized at POCA -
      // too much trouble at this point
      reco::perigee::Parameters h(0,trackP3.theta(),trackP3.phi(),0,0,trackP3.perp());
      reco::perigee::Covariance c;

      // TrackBase( double chi2, double ndof, 
      //            int found, int invalid, int lost,
      //            const Parameters &, const Covariance & );
      
      reco::TestMuon aMuon(0, 0, 0, 0, 0, h, c);
      
      TrackAssociator::AssociatorParameters parameters;
      parameters.useEcal = useEcal_ ;
      parameters.useHcal = useHcal_ ;
      parameters.useMuon = useMuon_ ;
      parameters.dRHcal = 0.4;
      parameters.dRHcal = 0.4;

      TrackDetMatchInfo info = trackAssociator_.associate(iEvent, iSetup, 
							  trackAssociator_.getFreeTrajectoryState(iSetup, *tracksCI, vertex),
							  parameters);
      reco::TestMuon::MuonEnergy muonEnergy;
      muonEnergy.had = info.hcalEnergy();
      muonEnergy.em = info.ecalEnergy();
      muonEnergy.ho = info.outerHcalEnergy();
      aMuon.setCalEnergy( muonEnergy );
      
      reco::TestMuon::MuonIsolation muonIsolation;
      muonIsolation.hCalEt01 = 0;
      muonIsolation.eCalEt01 = 0;
      muonIsolation.hCalEt04 = info.hcalConeEnergy();
      muonIsolation.eCalEt04 = info.ecalConeEnergy();
      muonIsolation.hCalEt07 = 0;
      muonIsolation.eCalEt07 = 0;
      muonIsolation.trackSumPt01 = 0;
      muonIsolation.trackSumPt04 = 0;
      muonIsolation.trackSumPt07 = 0;
      aMuon.setIsolation( muonIsolation );
      
      std::vector<reco::TestMuon::MuonMatch> muonMatches;
      for( std::vector<MuonSegmentMatch>::const_iterator segment=info.segments.begin();
	   segment!=info.segments.end(); segment++ )
	{
	   reco::TestMuon::MuonMatch aMatch;
	   aMatch.dX = segment->segmentLocalPosition.x()-segment->trajectoryLocalPosition.x();
	   aMatch.dY = segment->segmentLocalPosition.y()-segment->trajectoryLocalPosition.y();
	   aMatch.dXErr = sqrt(segment->trajectoryLocalErrorXX+segment->segmentLocalErrorXX);
	   aMatch.dYErr = sqrt(segment->trajectoryLocalErrorYY+segment->segmentLocalErrorYY);
	   aMatch.dXdZ = 0;
	   aMatch.dYdZ = 0;
	   aMatch.dXdZErr = 0;
	   aMatch.dYdZErr = 0;
	   muonMatches.push_back(aMatch);
	   std::cout<< "Muon match (dX,dY,dXErr,dYErr): " << aMatch.dX << " \t" << aMatch.dY 
	     << " \t" << aMatch.dXErr << " \t" << aMatch.dYErr << std::endl;
	}
      aMuon.setMatches(muonMatches);
      std::cout << "number of muon matches: " << aMuon.matches().size() << std::endl;
      outputMuons->push_back(aMuon);
   }
   iEvent.put(outputMuons,"muons");
}

//define this as a plug-in
DEFINE_FWK_MODULE(TestMuonProducer)
