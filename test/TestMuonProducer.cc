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
// $Id: TestMuonProducer.cc,v 1.2 2006/08/10 13:33:37 dmytro Exp $
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
   produces<reco::TrackCollection>("tracks");

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
   
   std::auto_ptr<reco::TrackCollection> outputTracks(new reco::TrackCollection);
   RefProd<reco::TrackCollection> refOutputTracks = iEvent.getRefBeforePut<reco::TrackCollection>("tracks");
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
   
   uint i(0);
   // loop over simulated tracks
   for(SimTrackContainer::const_iterator tracksCI = simTracks->begin(); 
       tracksCI != simTracks->end(); tracksCI++){
      // get vector
      Hep3Vector trackP3 = tracksCI->momentum().vect();

      // skip low Pt tracks
      if (tracksCI->momentum().perp() < 5) {
	 std::cout << "Skipped low Pt track (Pt: " << tracksCI->momentum().perp() << ")" <<std::endl;
	 continue;
      }
      
      // get vertex
      int vertexIndex = tracksCI->vertIndex();
      // uint trackIndex = tracksCI->genpartIndex();
      
      SimVertex vertex(Hep3Vector(0.,0.,0.),0);
      if (vertexIndex >= 0) vertex = (*simVertices)[vertexIndex];

      // skip tracks originated away from the IP
      if (vertex.position().rho() > 50) {
	 std::cout << "Skipped track originated away from IP: " <<vertex.position().rho()<<std::endl;
	 continue;
      }
      
      // Ignore vertex. The track can only be initialized at POCA -
      // too much trouble at this point
      reco::TrackBase::ParameterVector par(0,trackP3.theta(),trackP3.phi(),0,0);
      reco::TrackBase::CovarianceMatrix c;

      // TrackBase( double chi2, double ndof,
      // const ParameterVector & par, double pt, const CovarianceMatrix & cov )
      
      // 
      reco::Track aTrack(0, 0, par, trackP3.perp() , c);
      outputTracks->push_back(aTrack);
      
      reco::TestMuon aMuon;
      // aMuon.setTrack(reco::TrackRef(trackHandle,i));
      aMuon.setTrack(reco::TrackRef(refOutputTracks,i));
      i++;
      
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
   iEvent.put(outputTracks,"tracks");
   iEvent.put(outputMuons,"muons");
}

//define this as a plug-in
DEFINE_FWK_MODULE(TestMuonProducer)
