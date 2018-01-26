#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include <assert.h>

using namespace std;

struct FTBFPixelEvent {
    double xSlope;
    double ySlope;
    double xIntercept;
    double yIntercept;
    double chi2;
    int trigger;
    int runNumber;
    Long64_t timestamp;    
    Long64_t bco;    
};


std::string ParseCommandLine( int argc, char* argv[], std::string opt )
{
  for (int i = 1; i < argc; i++ )
    {
      std::string tmp( argv[i] );
      if ( tmp.find( opt ) != std::string::npos )
        {
          if ( tmp.find( "=" )  != std::string::npos ) return tmp.substr( tmp.find_last_of("=") + 1 );
	  if ( tmp.find( "--" ) != std::string::npos ) return "yes";
	}
    }
  
  return "";
};



//get list of files to open, add normalization branch to the tree in each file
int main(int argc, char* argv[]) {

  //parse input list to get names of ROOT files
  if(argc < 4){
    cerr << "usage MergeTOFPETWithDRS <PixelFile> <TrackerTimingFile> <TOFPETEventFile> <OutputFile>" << endl;
    return -1;
  }
  int debugLevel = -1;
  debugLevel = -1;
  const char *PixelInputFileName = argv[1];
  const char *TrackerTimingInputFileName = argv[2];
  const char *TOFPETEventFileName   = argv[3];
  const char *outFileName   = argv[4];

  cout << "PixelInputFile = " << PixelInputFileName << "\n";
  cout << "TrackerTimingInputFileName = " << TrackerTimingInputFileName << "\n";
  cout << "TOFPETEventFileName = " << TOFPETEventFileName << "\n";
  cout << "outFileName = " << outFileName << "\n";

  const char *NimPlusInputFileName = "/eos/uscms/store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/NimPlus/RawDataSaver0NIM0_Run1771_0_Raw.dat";

  //**********************************
  //First read in the NimPlus timestamps --> We don't use this anymore because it was giving errors
  //**********************************
  long triggerNumber=-1; 
  vector<long long> NimPlusTimestamp;
  vector<long long> NimPlusTimestampDelay;
  FILE* NimPlusInputFile = fopen( NimPlusInputFileName, "rb" );
  //cout << NimPlusInputFileName << endl;
  //int cnt = 0;
  //while(!feof(NimPlusInputFile) && cnt++ < 5)
  //{

  //  cout << fgetc(NimPlusInputFile) << endl;
  // }
  //return 0;
  //ifstream myfile;
  //myfile.open(NimPlusInputFileName, ios::in | ios::binary);
  int eventWordIndex=0;
  long long firstTimeEver = 0;

  firstTimeEver = 0x89abcdef;

  cout << "BYTE order test" << endl;
  cout << hex << firstTimeEver << dec << endl;
  for(int i=0;i<4;++i)
    {
      cout << hex << (unsigned int)(((unsigned char *)(&firstTimeEver))[i]) << dec << " -----\n";
      (((unsigned char *)(&firstTimeEver))[i]) = i;
    }
  cout << hex << firstTimeEver << dec << endl;

  for(int i=0;i<4;++i)
    {
      cout << hex << (unsigned int)(((unsigned char *)(&firstTimeEver))[i]) << dec << " -----\n";
      (((unsigned char *)(&firstTimeEver))[i]) = 0;
    }
  cout << hex << firstTimeEver << dec << endl;

  firstTimeEver = 0;

  bool isNewTrigger = false;
  int maxPackets = 999999999;
  for( int iPacket = 0; iPacket < maxPackets; iPacket++){ 
  //while(!feof(NimPlusInputFile)){
    long tmpTrigger = 0;
    long long tmpTimestampPart1 = 0;
    long long tmpTimestampPart2 = 0;
    long long tmpWord = 0;
    
    long tmp = 0;
    long QuadNo=0;
    long long t_diff=0;
    //cout << "Event: " << iPacket << " : ";    

    int x;
    // int k=-4;
    //if(iPacket<5){myfile>>x;cout<<static_cast<long>(x)<<endl;}
    //if(iPacket<5){myfile>>x;cout<<static_cast<long>(x)<<endl;}
    //unsigned char tmpC; 
    fread( &QuadNo, 1, 1, NimPlusInputFile); //no. of quad words in each packet (1 quadword= 8 bytes= 64 bits)
    if (debugLevel > 100) cout <<QuadNo << " ";
    fread( &tmp, 1, 1, NimPlusInputFile); //packet type -- 1,2 or 3
    if (debugLevel > 100) cout << tmp << " ";
    fread( &tmp, 1, 1, NimPlusInputFile); // sequence ID -- increments by 1 each time
    if (debugLevel > 100) cout << tmp << "\n ";
    for(int i=0;i<QuadNo*2;i++){

      //read 32-bit words
      tmpWord = 0;
      fread( &tmpWord, sizeof(float), 1, NimPlusInputFile); 
      if (debugLevel > 100) cout << "\t" << (eventWordIndex%6) << "-" <<  tmpWord << " ";
      //cout << "(k=" << eventWordIndex << ") ";

      //this is the trigger number word
      if (eventWordIndex%6==2) {
  	//a new trigger
  	if (tmpWord > triggerNumber) {
  	  triggerNumber++;
  	  isNewTrigger = true;
  	  //if (debugLevel > 10) cout << "Trigger Number: " << tmpWord << " : ";
  	}
      }

      //first 32-bit word-part of the timestamp
      if(eventWordIndex%6==4){
  	if (isNewTrigger) {
	  
  	  // if(firstTimeEver == 0 || 
  	  //    (NimPlusTimestamp.size() && 
  	  //     (tmpWord-NimPlusTimestamp[NimPlusTimestamp.size()-1])*3 > 1000000000))
  	  //   firstTimeEver = tmpWord;

  	  // if(NimPlusTimestamp.size() &&
  	  //     NimPlusTimestamp[NimPlusTimestamp.size()-1] > tmpWord) {
  	  //     cout << "????";
  	  //     //tmpWord += ((long long)(1)<<32);
  	  // }
  	  tmpTimestampPart1 = tmpWord;	    	   
  	}      
      }

      if(eventWordIndex%6==5){
  	if (isNewTrigger) {  	
  	  tmpTimestampPart2 = tmpWord;
	  
  	  if (debugLevel > 100) cout << "-0x" << hex << tmpTimestampPart1 << dec << "\t\t-DIFF=" << 
  				  ((tmpTimestampPart1-firstTimeEver)*3.0f)/1000000.0f << "ms ";
  	  NimPlusTimestamp.push_back(tmpTimestampPart2*4294967296 + tmpTimestampPart1);
  	  if (debugLevel > 100) cout << "\t FullTimeStamp = " << (tmpTimestampPart2*4294967296 + tmpTimestampPart1);
  	  isNewTrigger = false;

  	}
	
  	if (debugLevel > 100) cout << endl;
      }

      if(eventWordIndex%6==5){
  	
      }
      eventWordIndex++;

    }
    if (debugLevel > 100) cout << "\n--------------------------------------------------------------------------------\n";
    // // check for end of file
    if (feof(NimPlusInputFile)) break;
  }

  //timestamps are in units of clock cycles (3ns each step)
  long long tmpRunningTimestamp = 0;
  for (int i=0; i<NimPlusTimestamp.size();i++) {
    // cout << "Trigger: " << i << " " << NimPlusTimestamp[i] << "\n";
    if (i==0) {
      NimPlusTimestampDelay.push_back(0);
    } else {
      //if (NimPlusTimestamp[i] - NimPlusTimestamp[i-1] > 0) {
  	NimPlusTimestampDelay.push_back( (NimPlusTimestamp[i] - NimPlusTimestamp[i-1]) * 3); //delays are in units of ns
  	//} else {
  	//NimPlusTimestampDelay.push_back( (NimPlusTimestamp[i] + 4294967296 - NimPlusTimestamp[i-1]) * 3);
  	//}
    }
    // cout << "Trigger: " << i << " | " << NimPlusTimestamp[i] << " | " 
    // 	 << (tmpRunningTimestamp +  NimPlusTimestampDelay[i])*1e-9 << " : " 
    // 	 << NimPlusTimestampDelay[i]*1e-9 << "\n";
    if (i>0 && NimPlusTimestamp[i] - NimPlusTimestamp[i-1] < 10000) {
      cout << "Trigger: " << i << " | " << NimPlusTimestamp[i] << " | " 
	   << NimPlusTimestamp[i] - NimPlusTimestamp[i-1] << " "
	   << "\n";
    }
    //if (i>1) {
      tmpRunningTimestamp +=  NimPlusTimestampDelay[i];
      //}
  }



  return 0;

  //*************************************************************
  // Read the Timestamps from the tracker
  //*************************************************************
  ifstream TrackerTimingInputFile( TrackerTimingInputFileName );
  assert (TrackerTimingInputFile.is_open());
  vector<double> TrackerTimestamps;
  vector<double> TrackerTimestampsBCO;  
  string line;
  while ( getline (TrackerTimingInputFile,line) ) {
    istringstream iss(line);
    string temp;
    long tempBCOstamp;
    double tempTimestamp;
    iss >> temp >> temp >> temp >> tempBCOstamp >> temp >> tempTimestamp;
    //TrackerTimestamps.push_back(tempTimestamp);
    TrackerTimestampsBCO.push_back(tempBCOstamp);
  }
  TrackerTimingInputFile.close();
  
  for (int i=0; i<TrackerTimestampsBCO.size();i++) {
    if (TrackerTimestampsBCO[i] >= TrackerTimestampsBCO[0]) {
      TrackerTimestamps.push_back( (TrackerTimestampsBCO[i]-TrackerTimestampsBCO[0])*76.7990858*1e-9);
    } else {
       TrackerTimestamps.push_back( (TrackerTimestampsBCO[i] + 281474976710656 -TrackerTimestampsBCO[0])*76.7990858*1e-9);     
    }
    cout.precision(10);
    //cout << "Timestamp : " << i << " : " << TrackerTimestamps[i] << "\n";
  }

  //reset the counter whenever there is more than 1 second between one event and the next
  vector<double> TrackerTimestampsResetted;
  int previousResetIndex = 0;
  for (int i=0; i<TrackerTimestamps.size();i++) {    
    TrackerTimestampsResetted.push_back(TrackerTimestamps[i]);
    if (i==0) continue;    
    //cout << "test: " << i << " : " << TrackerTimestamps[i] << " " << TrackerTimestamps[i-1] << " " << TrackerTimestamps[i] - TrackerTimestamps[i-1] << "\n";
    if (TrackerTimestamps[i] - TrackerTimestamps[i-1] > 1.0) previousResetIndex = i;
    TrackerTimestampsResetted[i] = TrackerTimestamps[i] - TrackerTimestamps[previousResetIndex];
    //cout << "Tracker Timer: " << i << " : " << TrackerTimestampsResetted[i] << "\n";
  }
  

  //*************************************************************
  // Read the TOFPET file
  //*************************************************************

  //create output file
  TFile *outputFile = new TFile(outFileName, "RECREATE");

  //loop over all TTrees in the file and add the weight branch to each of them
  TFile *PixelInputFile = TFile::Open(PixelInputFileName, "READ");
  TFile *TOFPETEventFile = TFile::Open(TOFPETEventFileName, "READ");
  assert(PixelInputFile);
  PixelInputFile->cd();
  assert(TOFPETEventFile);

  TTree *PixelTree = (TTree*)PixelInputFile->Get("MAPSA");
  TTree *TOFPETEventTree = (TTree*)TOFPETEventFile->Get("data");
 
  //create new normalized tree
  outputFile->cd();
  TTree *outputTree = TOFPETEventTree->CloneTree(0);
  //cout << "Events in the ntuple: " << PixelTree->GetEntries() << endl;
  
  //branches for the TOFPET tree
  UShort_t outputCHID[64];
  Long64_t chTime[64];
  float chEnergy[64];
  float chTqT[64];
  Int_t event;
  // outputTree->Branch("chID","vector <UShort_t>",&chID);
  outputTree->Branch("chID",&outputCHID,"chID[64]/s");
  outputTree->Branch("chTime",&chTime,"chTime[64]/L");
  outputTree->Branch("chTqT",&chTqT,"chTqT[64]/F");
  outputTree->Branch("chEnergy",&chEnergy,"chEnergy[64]/F");
  TOFPETEventTree->SetBranchAddress("event",&event);
  TOFPETEventTree->SetBranchAddress("chTime",&chTime);
  TOFPETEventTree->SetBranchAddress("chTqT",&chTqT);
  TOFPETEventTree->SetBranchAddress("chEnergy",&chEnergy);

  //branches for the PixelTree
  FTBFPixelEvent pixelEvent;
  PixelTree->SetBranchAddress("event",&pixelEvent);
  float xIntercept;
  float yIntercept;
  float xSlope;
  float ySlope;
  float x1;
  float y1;
  float x2;
  float y2;
  int ntracks;
  outputTree->Branch("xIntercept", &xIntercept, "xIntercept/F");
  outputTree->Branch("yIntercept", &yIntercept, "yIntercept/F");
  outputTree->Branch("xSlope", &xSlope, "xSlope/F");
  outputTree->Branch("ySlope", &ySlope, "ySlope/F");
  outputTree->Branch("x1", &x1, "x1/F");
  outputTree->Branch("y1", &y1, "y1/F");
  outputTree->Branch("x2", &x2, "x2/F");
  outputTree->Branch("y2", &y2, "y2/F");
  outputTree->Branch("ntracks", &ntracks, "ntracks/I");


  vector<long long> TOFPETTimestamp;
  vector<long long> TOFPETTimestampDelay;
  long long previousTimestamp_TOFPET = 0;
  long long previousTimestamp2_TOFPET = 0;
  long long previousTimestamp3_TOFPET = 0;
  long long previousTimestamp4_TOFPET = 0;
  long long previousTimestamp5_TOFPET = 0;
  long long previousTimestamp6_TOFPET = 0;
  long long FirstEventTimeTOFPET = 0;
  int TriggerIndexTOFPET = 0;
  int PreviouslyMatchedTriggerNumber = -1;
  int TOFPETPreviousResetIndex = 0;
  double TOFPETPreviousResetTime = 0;
  for(int q=0; q < TOFPETEventTree->GetEntries(); q++) {

    TOFPETEventTree->GetEntry(q);
        
    int MatchedTriggerIndex = -1;
    if (chEnergy[32] != -9999) {
      TOFPETTimestamp.push_back(chTime[32]);
      if (TriggerIndexTOFPET == 0) {
	FirstEventTimeTOFPET = chTime[32];
	TOFPETPreviousResetTime = chTime[32]*1e-12*0.96;
	previousTimestamp_TOFPET = chTime[32];
      }
      double timeElapsedSincePreviousTrigger = (chTime[32] - previousTimestamp_TOFPET)* 1e-12 * 0.96;
      previousTimestamp_TOFPET = chTime[32];
 
      double timeElapsed = (chTime[32] - FirstEventTimeTOFPET)*1e-12*0.96; //0.96 factor is required to match NIMPlus clock speed.
      
      //reset timer if more than 1 second since last trigger
      if (timeElapsedSincePreviousTrigger > 1.0) {
	TOFPETPreviousResetIndex = q;
	TOFPETPreviousResetTime = timeElapsed;
      }

      double timeElapsedResetted = 0;
      if (TriggerIndexTOFPET > 0) timeElapsedResetted = timeElapsed - TOFPETPreviousResetTime;


      if (debugLevel > 100) {
	cout << "Trigger:\t " << TriggerIndexTOFPET << " \t " << chTime[32] << " \t " 
  	   << timeElapsed << " \t " 
  	   << timeElapsedResetted << " \t " 
	   << timeElapsedSincePreviousTrigger << "\t"
	   << TOFPETPreviousResetTime << "\t"
	   << "\n"; 
      }


      //Try to find matching trigger in Tracking timestamps
      bool stopSearching = false;
      for (int p=PreviouslyMatchedTriggerNumber+1; p<TrackerTimestamps.size() && !stopSearching; p++) {
	if (debugLevel > 100) {
	  cout << "TOFPET Event " << q << " " << timeElapsed << " , " << timeElapsedResetted 
	       << " -> " << p << " " << TrackerTimestamps[p] << " , " << TrackerTimestampsResetted[p] 
	       << " | " << fabs(TrackerTimestamps[p]-timeElapsed) << " , " 
	       << fabs(TrackerTimestampsResetted[p] - timeElapsedResetted) << " "
	       << "\n";
	}
	if ( fabs(TrackerTimestamps[p]-timeElapsed) < 0.1
	     && fabs(TrackerTimestampsResetted[p] - timeElapsedResetted) < 0.0005
	     ) {	 
	  if (debugLevel > 100) cout << "matched\n";
	  MatchedTriggerIndex = p;
	  PreviouslyMatchedTriggerNumber = p;
	  break;
	} else {
	  if (debugLevel > 100) cout << "BAD!!\n";
	  if ( TrackerTimestamps[p]-timeElapsed > 0.1) {
	    stopSearching = true;
	  }
	}
      }

      if (debugLevel > 100) {
	cout << "Matched : " << MatchedTriggerIndex << "\n";
	if (stopSearching) cout << "stopped searching, skip events\n";
      }
      
      TriggerIndexTOFPET++;

    }


    //****************************************
    // Write output if matched
    //****************************************
    if (MatchedTriggerIndex >= 0) {
      ntracks = 0;
      for( int iPixelEvent = 0; iPixelEvent < PixelTree->GetEntries(); iPixelEvent++){ 
	PixelTree->GetEntry(iPixelEvent);
	if (pixelEvent.trigger == MatchedTriggerIndex) {
	  xIntercept = pixelEvent.xIntercept;
	  yIntercept = pixelEvent.yIntercept;
	  xSlope = pixelEvent.xSlope;
	  ySlope = pixelEvent.ySlope;
	  x1 = xIntercept + xSlope*(-50000);
	  y1 = yIntercept + ySlope*(-50000);
	  x2 = xIntercept + xSlope*(50000);
	  y2 = yIntercept + ySlope*(50000);
	  ntracks++;
	}
      } //loop over all pixel events
    } else {
      xIntercept = -999;
      yIntercept = -999;
      xSlope = -999;
      ySlope = -999;
      x1 = -999;
      y1 = -999;
      x2 = -999;
      y2 = -999;  
      ntracks = -1;
    }

    //Fill output tree
    outputTree->Fill();
  } // loop over TOFPET events
  
 
  //save
  outputTree->Write();

  //Close input files
  PixelInputFile->Close();
  TOFPETEventFile->Close();
  //cout << "Closing output file." << endl;
  outputFile->Close();
  delete outputFile;
}
