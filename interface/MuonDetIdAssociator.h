#ifndef TrackAssociator_MuonDetIdAssociator_h
#define TrackAssociator_MuonDetIdAssociator_h 1
// -*- C++ -*-
//
// Package:    TrackAssociator
// Class:      MuonDetIdAssociator
// 
/*

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Dmytro Kovalskyi
//         Created:  Fri Apr 21 10:59:41 PDT 2006
// $Id: MuonDetIdAssociator.h,v 1.2.12.1 2007/10/06 05:50:12 jribnik Exp $
//
//

#include "TrackingTools/TrackAssociator/interface/DetIdAssociator.h"
#include "TrackingTools/TrackAssociator/interface/MuonChamberMatch.h"
#include "TrackPropagation/SteppingHelixPropagator/interface/SteppingHelixStateInfo.h"
#include "Geometry/CommonDetUnit/interface/GlobalTrackingGeometry.h"
#include "Geometry/CommonDetUnit/interface/GeomDet.h"
#include "DataFormats/DetId/interface/DetId.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

class MuonDetIdAssociator: public DetIdAssociator{
 public:
   MuonDetIdAssociator():DetIdAssociator(48, 48 , 0.125),geometry_(0){};
   MuonDetIdAssociator(const int nPhi, const int nEta, const double etaBinSize)
     :DetIdAssociator(nPhi, nEta, etaBinSize),geometry_(0){};

   MuonDetIdAssociator(const edm::ParameterSet& pSet)
     :DetIdAssociator(pSet.getParameter<int>("nPhi"),pSet.getParameter<int>("nEta"),pSet.getParameter<double>("etaBinSize")),geometry_(0){};
   
   virtual void setGeometry(const GlobalTrackingGeometry* ptr){ geometry_ = ptr; }

   virtual void setGeometry(const DetIdAssociatorRecord& iRecord){
      edm::ESHandle<GlobalTrackingGeometry> geometryH;
      iRecord.getRecord<GlobalTrackingGeometryRecord>().get(geometryH);
      setGeometry(geometryH.product());
   };
   
   virtual const GeomDet* getGeomDet( const DetId& id ) const;

 protected:
   
   virtual void check_setup() const;
   
   virtual GlobalPoint getPosition(const DetId& id) const;
   
   virtual std::set<DetId> getASetOfValidDetIds() const;
   
   virtual std::vector<GlobalPoint> getDetIdPoints(const DetId& id) const;

   virtual bool insideElement(const GlobalPoint& point, const DetId& id) const;

   const GlobalTrackingGeometry* geometry_;
};
#endif
