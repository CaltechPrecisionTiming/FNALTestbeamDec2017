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
bool found_corruption(FILE* stream, size_t count){
  if(count < 1)
    return false;

  uint event_header;
  int dummy;
  
  for(size_t i = 0; i < count; i++){
    dummy = fread( &event_header, sizeof(uint), 1, stream);
    if(event_header == 0){
      fseek(stream, -3*sizeof(uint), SEEK_CUR);
      return true;
    }
  } 
  // rewind
  fseek(stream, -count*sizeof(uint), SEEK_CUR);
  return false;
}
unsigned createMask(unsigned a, unsigned b)
{
  unsigned r = 0;
  for (unsigned i=a; i<=b; i++)
    r |= 1 << i;

  return r;
}

//get list of files to open, add normalization branch to the tree in each file
int main(int argc, char* argv[]) {

  //parse input list to get names of ROOT files
  if(argc < 4){
    cerr << "usage MergeTOFPETWithDRS <PixelFile> <TOFPETEventFile> <DRSFile> <OutputFile>" << endl;
    return -1;
  }
  int debugLevel = -1;
  debugLevel = -1;


  int dummy;
  // temp variables for data input
  uint   event_header;
  uint   temp[3];
  ushort samples[9][1024];

  const char *PixelInputFileName = argv[1];
  const char *TOFPETEventFileName   = argv[2];
  const char *DRSFileName   = argv[3];
  const char *outFileName   = argv[4];

  cout << "PixelInputFile = " << PixelInputFileName << "\n";
  cout << "TOFPETEventFileName = " << TOFPETEventFileName << "\n";
  cout << "DRSFileName = " << DRSFileName << "\n";
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
  /* for( int iPacket = 0; iPacket < maxPackets; iPacket++){ 
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



  return 0;*/
  

  //*************************
  // Open Input DRS File
  //************************* 

  FILE* DRSFile = fopen( DRSFileName, "r" );

  //*************************
  //Event Loop
  //*************************
  
  //define timestamps
  vector<long long > DRSTimestamp;
  vector<long long> DRSTimestampDelay;
  vector<int> DRSspillIndex;
  int maxEvents = 999999;
  for( int iEvent = 0; iEvent < maxEvents; iEvent++){ 
  // first header word
  dummy = fread( &event_header, sizeof(uint), 1, DRSFile);
  // second header word
  dummy = fread( &event_header, sizeof(uint), 1, DRSFile);  
  uint grM     = event_header & 0x0f; // 4-bit channel group mask
  // third and fourth header words
  dummy = fread( &event_header, sizeof(uint), 1, DRSFile);  
  dummy = fread( &event_header, sizeof(uint), 1, DRSFile); 

  // check for end of file
  if (feof(DRSFile)) break;
    
  //*************************
  // Parse group mask into channels
  //*************************

  bool _isGR_On[4];
  _isGR_On[0] = (grM & 0x01);
  _isGR_On[1] = (grM & 0x02);
  _isGR_On[2] = (grM & 0x04);
  _isGR_On[3] = (grM & 0x08);
    
  // hard-coded for two DRS4 chips/groups
  _isGR_On[0] = 1;
  _isGR_On[1] = 1;
  _isGR_On[2] = 0;
  _isGR_On[3] = 0;
  int activeGroupsN = 0;
  int realGroup[4] = {-1, -1, -1, -1};
  for ( int l = 0; l < 4; l++ ) {
    if ( _isGR_On[l] ) 
      {
	realGroup[activeGroupsN] = l; 
	activeGroupsN++;
      }
  }


  // check for corrupted binary data
  bool corruption = false;

  //************************************
  // Loop over channel groups
  //************************************
  //std::cout << activeGroupsN << " activeGroupsN" << std::endl;
  for ( int group = 0; group < activeGroupsN; group++ ) {
    // Read group header
    if(found_corruption(DRSFile, 1)){
      corruption = true;
      break;
    }
    dummy = fread( &event_header, sizeof(uint), 1, DRSFile); 
 
    // ushort tcn = (event_header >> 20) & 0xfff; // trigger counter bin
    //tc[realGroup[group]] = tcn;
       
    // Check if all channels were active (if 8 channels active return 3072)
    int nsample = (event_header & 0xfff) / 3;

    // hard-coded to protect against corruption
    nsample = 1024;

    //************************************
    // Read sample info for group
    //************************************      

    for ( int i = 0; i < nsample; i++ ) {
      if(found_corruption(DRSFile, 3)){
	corruption = true;
	break;
      }
      dummy = fread( &temp, sizeof(uint), 3, DRSFile );  

      samples[0][i] =  temp[0] & 0xfff;
      samples[1][i] = (temp[0] >> 12) & 0xfff;
      samples[2][i] = (temp[0] >> 24) | ((temp[1] & 0xf) << 8);
      samples[3][i] = (temp[1] >>  4) & 0xfff;
      samples[4][i] = (temp[1] >> 16) & 0xfff;
      samples[5][i] = (temp[1] >> 28) | ((temp[2] & 0xff) << 4);
      samples[6][i] = (temp[2] >>  8) & 0xfff;
      samples[7][i] =  temp[2] >> 20;
    }

    if(corruption) break;
    
    // Trigger channel
    for(int j = 0; j < nsample/8; j++){
      if(found_corruption(DRSFile, 3)){
	corruption = true;
	break;
      }
      fread( &temp, sizeof(uint), 3, DRSFile);  

      samples[8][j*8+0] =  temp[0] & 0xfff;
      samples[8][j*8+1] = (temp[0] >> 12) & 0xfff;
      samples[8][j*8+2] = (temp[0] >> 24) | ((temp[1] & 0xf) << 8);
      samples[8][j*8+3] = (temp[1] >>  4) & 0xfff;
      samples[8][j*8+4] = (temp[1] >> 16) & 0xfff;
      samples[8][j*8+5] = (temp[1] >> 28) | ((temp[2] & 0xff) << 4);
      samples[8][j*8+6] = (temp[2] >>  8) & 0xfff;
      samples[8][j*8+7] =  temp[2] >> 20;
    }

    if(corruption) break;

    if(found_corruption(DRSFile, 1)){
      corruption = true;
      break;
    }
    dummy = fread( &event_header, sizeof(uint), 1, DRSFile);

    if (group == 0) {
      unsigned bitmask = createMask(0,30);
      unsigned result = bitmask & event_header;
      //cout << "Group Trigger time: " << event_header << " : " << bitmask << " : " << result << "\n";
      DRSTimestamp.push_back(result);
    }
  }
    
  // encountered a header for another event while
  // processing this one, discarding it
  if(corruption) continue;
  }
   long long tmpRunningTimestamp = 0;
   vector <long long> DRSRunningTimestamp;
   for (int i=0; i<DRSTimestamp.size();i++) {
     if (i==0) {
       DRSTimestampDelay.push_back(0);
       DRSRunningTimestamp.push_back(0);
     } else {
       if (DRSTimestamp[i] - DRSTimestamp[i-1] > 0) {
   DRSTimestampDelay.push_back( (DRSTimestamp[i] - DRSTimestamp[i-1]) * 8.5); //delays are in units of s
       } else {
   DRSTimestampDelay.push_back( (DRSTimestamp[i] + 1073741824 - DRSTimestamp[i-1]) * 8.5);
       }
     }
     //cout << "Trigger: " << i << " | " << DRSTimestamp[i] << " | " 
     //<< (tmpRunningTimestamp +  DRSTimestampDelay[i])*1e-9 << " : " 
     //<< DRSTimestampDelay[i]*1e-9<< "\n";
     //if (i>1) {
       tmpRunningTimestamp +=  DRSTimestampDelay[i];
       DRSRunningTimestamp.push_back(tmpRunningTimestamp);
       //}
   }
  

    //reset the counter whenever there is more than 1 second between one event and the next
  vector<long long> DRSTimestampsResetted;
  int previousResetIndex = 0;
  for (int i=0; i<DRSTimestamp.size();i++) {    
    //DRSTimestampsResetted.push_back(DRSTimestamp[i]);
    if (i==0){
      DRSTimestampsResetted.push_back(0);
      continue;    
    }
    //cout << "test: " << i << " : " << DRSTimestamp[i]*8.5*1e-9 << " " << DRSTimestamp[i-1]*8.5*1e-9 << " " << (DRSTimestamp[i] - DRSTimestamp[i-1])*8.5*1e-9 << "\n";
    long long DRSReset=-9999;
    if (DRSTimestampDelay[i]*1e-9 > 0.05 ) previousResetIndex = i; 
    if(DRSTimestamp[i] - DRSTimestamp[previousResetIndex] > 0) DRSReset = (DRSTimestamp[i] - DRSTimestamp[previousResetIndex])*8.5;
    else DRSReset = (DRSTimestamp[i] + 1073741824 - DRSTimestamp[previousResetIndex])*8.5;
    
    DRSTimestampsResetted.push_back(DRSReset);
    //cout << "DRS Timer: " << i << " : " <<DRSTimestamp[i]*1e-9<< " : " <<DRSTimestamp[previousResetIndex]*1e-9<< " : " << DRSRunningTimestamp[i]*1e-9 <<" : "<<DRSTimestampsResetted[i]*1e-9 <<" : "<<DRSTimestampDelay[i]*1e-9<< "\n";
  } 
  for (int i=0; i<DRSTimestamp.size();i++){
    if(i==0 && DRSTimestampsResetted[1]*1e-9<0.05 && DRSTimestampsResetted[2]*1e-9<0.1){
      DRSspillIndex.push_back(0);
      DRSTimestampsResetted[i]=0;
      DRSTimestampDelay[i]=0;
    }
      
    if(DRSTimestampDelay[i]*1e-9 > 0.05 && DRSTimestampsResetted[i]*1e-9>4.0 && DRSTimestampsResetted[i+1]*1e-9<0.05 && DRSTimestampsResetted[i+2]*1e-9<0.1){
      DRSspillIndex.push_back(i);
      DRSTimestampsResetted[i]=0;
      DRSTimestampDelay[i]=0;
    }
    //cout << "DRS Timer: " << i << " : "<<DRSRunningTimestamp[i]*1e-9<<" : "<<DRSTimestampsResetted[i]*1e-9 <<" : "<<DRSTimestampDelay[i]*1e-9<< "\n";
  }
  
  //for (int i=0; i<DRSspillIndex.size();i++) cout<<"Spill "<<i+1<<" : Index - "<<DRSspillIndex[i]<<" ,Time - "<<DRSTimestamp[i]*1e-9<<" ,TimeElapsed - "<<DRSRunningTimestamp[i]*1e-9<<" "<<DRSTimestampDelay[i]*1e-9<<endl;

  //cout<<"====================================================================================================\n";
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


  vector<int>DRSpixMatch;

  for(int iEvent=0;iEvent < DRSTimestamp.size() ;iEvent++){
    bool foundMatch=false;
    for( int iPixelEvent = 0; iPixelEvent < PixelTree->GetEntries(); iPixelEvent++){ 
      PixelTree->GetEntry(iPixelEvent);
      //cout<<" Pixel Tigger: "<<pixelEvent.trigger<<" "<<iEvent<<endl;
      if(pixelEvent.trigger==iEvent){
	//cout<<"Found Match with DRS!\n";
	//cout<<pixelEvent.trigger<<endl;
	DRSpixMatch.push_back(iPixelEvent);
	foundMatch=true;
	break; // this drs event already matched, no need to further loop over all pixel events
      }
    }//pixel loop
    if(!foundMatch)DRSpixMatch.push_back(-9999);
  }
  vector<long long> TOFPETTimestamp;
  vector<long long> TOFPETTimestampDelay;
  long long previousTimestamp_TOFPET;
  long long FirstEventTimeTOFPET = 0;
  int TriggerIndexTOFPET = 0;
  int PreviouslyMatchedTriggerNumber = -1;
  int TOFPETPreviousResetIndex = 0;
  double TOFPETPreviousResetTime = 0;
  vector <int> TOFPETspillIndex;
  vector <double> timeElapsedSincePreviousTrigger;
  double tElapsedSincePreviousTrigger;
  vector <double> timeElapsed;
  vector <double> timeElapsedResetted;
  vector<int> TOFPETEvent;
  for(int q=0; q < TOFPETEventTree->GetEntries(); q++) {
    
    TOFPETEventTree->GetEntry(q);
     //if(q+1==TOFPETEventTree->GetEntries())cout<<q<<" "<<event<<endl;
    
    int MatchedTriggerIndex = -1;
   
    if (chEnergy[32] != -9999) {
      TOFPETTimestamp.push_back(chTime[32]);
      if (TriggerIndexTOFPET == 0) {
	FirstEventTimeTOFPET = chTime[32];
	TOFPETPreviousResetTime = 0;
	previousTimestamp_TOFPET = chTime[32];
	timeElapsedResetted.push_back(0);
      }
      tElapsedSincePreviousTrigger = (chTime[32] - previousTimestamp_TOFPET)*1e-12;
      timeElapsedSincePreviousTrigger.push_back((chTime[32] - previousTimestamp_TOFPET)*1e-12);
      previousTimestamp_TOFPET = chTime[32];
 
      double tElap=(chTime[32] - FirstEventTimeTOFPET)*1e-12;
      timeElapsed.push_back((chTime[32] - FirstEventTimeTOFPET)*1e-12); 
      
      //reset timer if more than 0.1 second since last trigger
      if (tElapsedSincePreviousTrigger > 0.1) {
	TOFPETPreviousResetIndex = q;
	TOFPETPreviousResetTime = tElap;
      }

      double timeElapsedReset = 0;
      if (TriggerIndexTOFPET > 0){
	timeElapsedReset = tElap - TOFPETPreviousResetTime;
	timeElapsedResetted.push_back(timeElapsedReset);
	 }
    
      TOFPETEvent.push_back(q);

      if (debugLevel > 100) {
	cout << "Trigger:\t " << TriggerIndexTOFPET << " \t " << chTime[32] << " \t " 
  	   << tElap << " \t " 
  	   << timeElapsedReset << " \t " 
	   << tElapsedSincePreviousTrigger << "\t"
	   << TOFPETPreviousResetTime << "\t"
	   << "\n"; 
	}
      TriggerIndexTOFPET++;
     
    }
  } // loop over TOFPET events
  for(int i=0;i<timeElapsedResetted.size();i++){
    if(i==0 && timeElapsedSincePreviousTrigger[i+1]<0.1 && timeElapsedSincePreviousTrigger[i+2]<0.1){
      TOFPETspillIndex.push_back(TOFPETEvent[i]);
      timeElapsedSincePreviousTrigger[i]=0;
      timeElapsedResetted[i]=0;
    }
    if(timeElapsedResetted[i]<4. && timeElapsedSincePreviousTrigger[i]>0.1 && timeElapsedSincePreviousTrigger[i+1]<0.1 && timeElapsedSincePreviousTrigger[i+2]<0.1){
      TOFPETspillIndex.push_back(TOFPETEvent[i]);
      timeElapsedSincePreviousTrigger[i]=0;
      timeElapsedResetted[i]=0;
    }
    /*cout << "Trigger:\t " << i  << " \t " 
  	   << timeElapsed[i] << " \t " 
  	   << timeElapsedResetted[i] << " \t " 
	   << timeElapsedSincePreviousTrigger[i] << "\t"
	   << "\n"; 
    */	

  }
  /*    for(int i=0; i<TOFPETspillIndex.size();i++){
    cout<<" TOFPET Event: "<<TOFPETspillIndex[i]<<" ";
    for(int j=0;j<TOFPETEvent.size();j++){
      if(TOFPETspillIndex[i]==TOFPETEvent[j])
	cout<<timeElapsed[j]<<" "<<timeElapsedResetted[j]<<" "<<timeElapsedSincePreviousTrigger[j]<<endl;
    }
    }*/
  for(int i=0; i<TOFPETspillIndex.size();i++){
    int evtstart=-99;
    for(int j=0;j<TOFPETEvent.size();j++){
      if(TOFPETspillIndex[i]==TOFPETEvent[j]){
	evtstart=j;
	break;
      }
    }
    PreviouslyMatchedTriggerNumber=DRSspillIndex[i]-1;
    //cout<<"==============================================================================\n";
    for(int k=evtstart;k<TOFPETEvent.size() && TOFPETEvent[k]<TOFPETspillIndex[i+1];k++){
      bool stopSearching=false;
      int MatchedTriggerIndex = -1;
      if(timeElapsedSincePreviousTrigger[k]<0.1 && timeElapsedResetted[k]<4.){
	double diff=9999;
	for(int iEvent=PreviouslyMatchedTriggerNumber+1;iEvent<DRSspillIndex[i+1] && !stopSearching;iEvent++){

	  if (debugLevel > 100) {
	    cout << "TOFPET Event " << k << " , " << timeElapsedResetted[k]<<" "<<timeElapsedSincePreviousTrigger[k]
		 << " -> " << iEvent << " , " << DRSTimestampsResetted[iEvent]*1e-9<<" "
		 <<DRSTimestampDelay[iEvent]*1e-9<<" | " 
	       << fabs(DRSTimestampsResetted[iEvent]*1e-9 - timeElapsedResetted[k]) << ", "
		 << fabs(DRSTimestampDelay[iEvent]*1e-9-timeElapsedSincePreviousTrigger[k])<<" "<<DRSpixMatch[iEvent]<<"\n";
	    }
	    if ( fabs(DRSTimestampsResetted[iEvent]*1e-9 - timeElapsedResetted[k])<0.002 && DRSpixMatch[iEvent]!=-9999 && diff>fabs(DRSTimestampsResetted[iEvent]*1e-9 - timeElapsedResetted[k])) {
	    PreviouslyMatchedTriggerNumber = iEvent;
	    MatchedTriggerIndex = iEvent;
	    //cout<<"xxxxxxxxxxxxxxx    Matched : "<<PreviouslyMatchedTriggerNumber<<endl;
	    diff=fabs(DRSTimestampsResetted[iEvent]*1e-9 - timeElapsedResetted[k]);
	  }
	  else{
	    if(fabs(DRSTimestampsResetted[iEvent]*1e-9 - timeElapsedResetted[k])>0.1)stopSearching=true;
	  }
	}//for DRS loop
      }//if(timeElapsedSincePreviousTrigger[k]<0.1 && timeElapsedResetted[k]<4.){
      TOFPETEventTree->GetEntry(TOFPETEvent[k]);
      if (MatchedTriggerIndex >= 0) {
       ntracks = 0;
       PixelTree->GetEntry(DRSpixMatch[MatchedTriggerIndex]);
       xIntercept = pixelEvent.xIntercept;
       yIntercept = pixelEvent.yIntercept;
       xSlope = pixelEvent.xSlope;
       ySlope = pixelEvent.ySlope;
       x1 = xIntercept + xSlope*(-50000);
       y1 = yIntercept + ySlope*(-50000);
       x2 = xIntercept + xSlope*(50000);
       y2 = yIntercept + ySlope*(50000);
       ntracks++;
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
    } // TOFPET event loop
  } // TOFPET spill loop
  //save
  outputTree->Write();

  //Close input files
  fclose(DRSFile);
  PixelInputFile->Close();
  TOFPETEventFile->Close();
  //cout << "Closing output file." << endl;
  outputFile->Close();
  delete outputFile;
    }
