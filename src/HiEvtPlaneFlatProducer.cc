// -*- C++ -*-
//
// Package:    HiEvtPlaneFlatProducer
// Class:      HiEvtPlaneFlatProducer
// 
/**\class HiEvtPlaneFlatProducer HiEvtPlaneFlatProducer.cc HiEvtPlaneFlatten/HiEvtPlaneFlatProducer/src/HiEvtPlaneFlatProducer.cc


 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Stephen Sanders
//         Created:  Sat Jun 26 16:04:04 EDT 2010
// $Id: HiEvtPlaneFlatProducer.cc,v 1.10 2011/11/06 23:17:46 ssanders Exp $
//
//
// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "Math/Vector3D.h"

#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/CaloTowers/interface/CaloTowerCollection.h"

#include "DataFormats/HeavyIonEvent/interface/Centrality.h"
#include "DataFormats/HeavyIonEvent/interface/CentralityBins.h"

#include "DataFormats/HeavyIonEvent/interface/EvtPlane.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "SimDataFormats/Track/interface/SimTrack.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/Track/interface/CoreSimTrack.h"
#include "SimDataFormats/EncodedEventId/interface/EncodedEventId.h"
#include "SimDataFormats/Vertex/interface/SimVertex.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "SimDataFormats/TrackingHit/interface/PSimHit.h"
#include "SimDataFormats/TrackingHit/interface/PSimHitContainer.h"
#include "SimDataFormats/TrackingHit/interface/UpdatablePSimHit.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingVertexContainer.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "CondFormats/DataRecord/interface/HeavyIonRPRcd.h"
#include "CondFormats/HIObjects/interface/CentralityTable.h"
#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HIObjects/interface/RPFlatParams.h"

#include "RecoHI/HiEvtPlaneAlgos/interface/HiEvtPlaneFlatten.h"
#include "TList.h"
#include "TString.h"
#include <time.h>
#include <cstdlib>

#include "RecoHI/HiEvtPlaneAlgos/interface/HiEvtPlaneList.h"

using namespace std;
using namespace hi;

#include <vector>
using std::vector;


//
// class declaration
//

class HiEvtPlaneFlatProducer : public edm::EDProducer {
   public:
      explicit HiEvtPlaneFlatProducer(const edm::ParameterSet&);
      ~HiEvtPlaneFlatProducer();

   private:
      virtual void beginJob() ;
      virtual void produce(edm::Event&, const edm::EventSetup&);
      virtual void endJob() ;
      
      // ----------member data ---------------------------

  edm::InputTag vtxCollection_;
  edm::InputTag inputPlanes_;
  edm::InputTag centrality_;

  int vs_sell;   // vertex collection size
  float vzr_sell;
  float vzErr_sell;
  bool FirstEvent;
  HiEvtPlaneFlatten * flat[NumEPNames];
  RPFlatParams * rpFlat;
  int nRP;
  bool storeNames_;

};

//
// constants, enums and typedefs
//
typedef std::vector<TrackingParticle>                   TrackingParticleCollection;
typedef TrackingParticleRefVector::iterator               tp_iterator;


//
// static data member definitions
//

//
// constructors and destructor
//
HiEvtPlaneFlatProducer::HiEvtPlaneFlatProducer(const edm::ParameterSet& iConfig)
{
  FirstEvent = kTRUE;
  vtxCollection_  = iConfig.getParameter<edm::InputTag>("vtxCollection_");
  inputPlanes_ = iConfig.getParameter<edm::InputTag>("inputPlanes_");
  centrality_ = iConfig.getParameter<edm::InputTag>("centrality_");
  storeNames_ = 1;
   //register your products
  produces<reco::EvtPlaneCollection>();
   //now do what ever other initialization is needed
  Int_t FlatOrder = 21;
  for(int i = 0; i<NumEPNames; i++) {
    flat[i] = new HiEvtPlaneFlatten();
    flat[i]->Init(FlatOrder,20,2,EPNames[i],EPOrder[i]);
    Double_t psirange = 4;
    if(EPOrder[i]==2 ) psirange = 2;
    if(EPOrder[i]==3 ) psirange = 1.5;
    if(EPOrder[i]==4 ) psirange = 1;
    if(EPOrder[i]==5) psirange = 0.8;
    if(EPOrder[i]==6) psirange = 0.6;
  }
  
}


HiEvtPlaneFlatProducer::~HiEvtPlaneFlatProducer()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

// ------------ method called to produce the data  ------------
void
HiEvtPlaneFlatProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;
  using namespace std;
  using namespace reco;

  //
  //Get Flattening Parameters
  //
  if(FirstEvent) {
    FirstEvent = kFALSE;
    edm::ESHandle<RPFlatParams> flatparmsDB_;
    iSetup.get<HeavyIonRPRcd>().get(flatparmsDB_);
    int flatTableSize = flatparmsDB_->m_table.size();
    for(int i = 0; i<flatTableSize; i++) {
      const RPFlatParams::EP* thisBin = &(flatparmsDB_->m_table[i]);
      for(int j = 0; j<NumEPNames; j++) {
	int indx = thisBin->RPNameIndx[j];
	if(indx>=0) {
	  flat[indx]->SetXDB(i, thisBin->x[j]);
	  flat[indx]->SetYDB(i, thisBin->y[j]);
	}
      }
    }
  }
  //
  //Get Centrality
  //
  
  edm::Handle<int> ch;
  iEvent.getByLabel(centrality_,ch);
  int bin = *(ch.product());

  //
  //Get Vertex
  //
  edm::Handle<reco::VertexCollection> vertexCollection3;
  iEvent.getByLabel(vtxCollection_,vertexCollection3);
  const reco::VertexCollection * vertices3 = vertexCollection3.product();
  vs_sell = vertices3->size();
  if(vs_sell>0) {
    vzr_sell = vertices3->begin()->z();
    vzErr_sell = vertices3->begin()->zError();
  } else
    vzr_sell = -999.9;
  
  //
  //Get Event Planes
  //
  
  Handle<reco::EvtPlaneCollection> evtPlanes;
  iEvent.getByLabel(inputPlanes_,evtPlanes);
  
  if(!evtPlanes.isValid()){
    //    cout << "Error! Can't get hiEvtPlane product!" << endl;
    return ;
  }
  double psiFull[NumEPNames];
  for(int i = 0; i<NumEPNames; i++) {
    psiFull[i] = -10;
  }

  std::auto_ptr<EvtPlaneCollection> evtplaneOutput(new EvtPlaneCollection);
  EvtPlane * ep[NumEPNames];
  for(int i = 0; i<NumEPNames; i++) {
    ep[i]=0;
  }
  for (EvtPlaneCollection::const_iterator rp = evtPlanes->begin();rp !=evtPlanes->end(); rp++) {
      string baseName = rp->label();
      for(int i = 0; i< NumEPNames; i++) {
	if(EPNames[i].compare(baseName)==0) {
	  double psiFlat = rp->angle();
	  if(psiFlat>-5) psiFlat = flat[i]->GetFlatPsi(rp->angle(),vzr_sell,bin);
	  if(EPNames[i].compare(rp->label())==0) {	    
	    psiFull[i] = psiFlat;
	    if(storeNames_) ep[i]= new EvtPlane(psiFlat, rp->sumSin(), rp->sumCos(),rp->label().data());
	    else ep[i]= new EvtPlane(psiFlat, rp->sumSin(), rp->sumCos(),"");
	  } 
	}
      }
  }
  
  for(int i = 0; i< NumEPNames; i++) {
    if(ep[i]!=0) evtplaneOutput->push_back(*ep[i]);
    
  }
  iEvent.put(evtplaneOutput);
  storeNames_ = 0;  
}

// ------------ method called once each job just before starting event loop  ------------
void 
HiEvtPlaneFlatProducer::beginJob()
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
HiEvtPlaneFlatProducer::endJob() {
}

//define this as a plug-in
DEFINE_FWK_MODULE(HiEvtPlaneFlatProducer);
