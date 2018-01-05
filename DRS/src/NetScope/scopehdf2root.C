#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <math.h>

// C++ includes
#include <fstream>
#include <string>
#include <iostream>

// ROOT includes
#include <TROOT.h>
#include <TStyle.h>
#include <TFile.h>
#include <TTree.h>
#include <TGraphErrors.h>
#include <TCanvas.h>

//LOCAL INCLUDES
#include "Aux.hh"

// NetScope INCLUDES
#include "common.h"
#include "hdf5io.h"

char *waveformBuf;

using namespace std;

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

int main(int argc, char **argv) {
  size_t i, j, iCh, iEvent=0, nEvents=0, frameSize, nEventsInFile;
  char *inFileName;

  // hardcoded number of time samples for pulses
  int NSAMPLES = 1000;

  // need to do a reverse ADC to use same
  // code as DRS, then reverse
  float ADC = (1.0 / 4096.0);
  
  struct hdf5io_waveform_file *waveformFile;
  struct waveform_attribute waveformAttr;
  struct hdf5io_waveform_event waveformEvent;
  
  if(argc<2) {
    std::cout << "Usage:" << std::endl;
    std::cout << "   " << argv[0] << " -ifile=input.hdf5 -ofile=output.root" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "   -iEvent=#event (starting event number)" << std::endl;
    std::cout << "   -nEvents=#events (total number of events to run over)" << std::endl;
    return EXIT_FAILURE;
  }

  std::string inputFilename = ParseCommandLine( argc, argv, "ifile" );
  std::cout << "Opening input file " << inputFilename << std::endl;
  waveformFile = hdf5io_open_file_for_read(inputFilename.c_str());

  std::string s_iEvent = ParseCommandLine( argc, argv, "iEvent" );
  if(s_iEvent != "")
    iEvent = atol(s_iEvent.c_str());
  
  std::string s_nEvents = ParseCommandLine( argc, argv, "nEvents" );
  if(s_nEvents != "")
    nEvents = atol(s_nEvents.c_str());
  
  hdf5io_read_waveform_attribute_in_file_header(waveformFile, &waveformAttr);
  fprintf(stderr, "waveform_attribute:\n"
	  "     chMask  = 0x%02x\n"
	  "     nPt     = %zd\n"
	  "     nFrames = %zd\n"
	  "     dt      = %g\n"
	  "     t0      = %g\n"
	  "     ymult   = %g %g %g %g\n"
	  "     yoff    = %g %g %g %g\n"
	  "     yzero   = %g %g %g %g\n",
	  waveformAttr.chMask, waveformAttr.nPt, waveformAttr.nFrames, waveformAttr.dt,
	  waveformAttr.t0, waveformAttr.ymult[0], waveformAttr.ymult[1], waveformAttr.ymult[2],
	  waveformAttr.ymult[3], waveformAttr.yoff[0], waveformAttr.yoff[1],
	  waveformAttr.yoff[2], waveformAttr.yoff[3], waveformAttr.yzero[0],
	  waveformAttr.yzero[1], waveformAttr.yzero[2], waveformAttr.yzero[3]);
  
  nEventsInFile = hdf5io_get_number_of_events(waveformFile);
  fprintf(stderr, "Number of events in file: %zd\n", nEventsInFile);
  if(nEvents <= 0 || nEvents+iEvent >= nEventsInFile) nEvents = nEventsInFile-iEvent;
  if(waveformAttr.nFrames > 0) {
    frameSize = waveformAttr.nPt / waveformAttr.nFrames;
    fprintf(stderr, "Frame size: %zd\n", frameSize);
  } else {
    frameSize = waveformAttr.nPt;
  }

  waveformBuf = (char*)malloc(waveformFile->nPt * waveformFile->nCh * sizeof(char));
  waveformEvent.wavBuf = waveformBuf;
  
  std::string outputFilename = ParseCommandLine( argc, argv, "ofile" );
  std::cout << "Initializing output file " << outputFilename << std::endl;
  TFile* ofile = new TFile( outputFilename.c_str(), "RECREATE", "NetScope");
  TTree* tree = new TTree("pulse", "Digitized waveforms");
  
  // initialization of variables for TTree output
  int event;
  float time[2][NSAMPLES]; // calibrated time
  float channel[4][NSAMPLES]; // input (in V)
  float xmin[4]; // location of peak
  float base[4]; // baseline voltage
  float amp[4]; // pulse amplitude
  float integral[4]; // integral in a window
  float integralFull[4]; // integral over all bins
  float gauspeak[4]; // time extracted with gaussian fit
  float sigmoidTime[4];//time extracted with sigmoid fit
  float fullFitTime[4];//time extracted with sigmoid fit
  float linearTime0[4]; // constant fraction fit coordinates
  float linearTime15[4];
  float linearTime30[4];
  float linearTime45[4];
  float linearTime60[4];
  float fallingTime[4]; // falling exponential timestamp    
  float risetime[4]; 
  float constantThresholdTime[4];
  bool _isRinging[4];

  float FFTtime[4];

  for(iCh=0; iCh<4; iCh++) {
    xmin[iCh] = 0.;
    amp [iCh] = 0.;
    base[iCh] = 0.;
    integral[iCh] = 0.;
    integralFull[iCh] = 0.;
    gauspeak[iCh] = 0.;
    sigmoidTime[iCh] = 0.;
    linearTime0[iCh] = 0.;
    linearTime15[iCh] = 0.;
    linearTime30[iCh] = 0.;
    linearTime45[iCh] = 0.;
    linearTime60[iCh] = 0.;
    risetime[iCh] = 0.;
    constantThresholdTime[iCh] = 0.;
    _isRinging[iCh] = false;
  }

  tree->Branch("event", &event, "event/I");
  tree->Branch("channel", channel, "channel[4][1000]/F");
  tree->Branch("time", time, "time[2][1000]/F");
  tree->Branch("xmin", xmin, "xmin[4]/F");
  tree->Branch("amp", amp, "amp[4]/F");
  tree->Branch("base", base, "base[4]/F");
  tree->Branch("integral", integral, "integral[4]/F");
  tree->Branch("intfull", integralFull, "intfull[4]/F");
  tree->Branch("gauspeak", gauspeak, "gauspeak[4]/F");
  tree->Branch("FFTtime", FFTtime, "FFTtime[4]/F");
  tree->Branch("sigmoidTime", sigmoidTime, "sigmoidTime[4]/F");
  tree->Branch("fullFitTime", fullFitTime, "fullFitTime[4]/F");
  tree->Branch("linearTime0", linearTime0, "linearTime0[4]/F");
  tree->Branch("linearTime15", linearTime15, "linearTime15[4]/F");
  tree->Branch("linearTime30", linearTime30, "linearTime30[4]/F");
  tree->Branch("linearTime45", linearTime45, "linearTime45[4]/F");
  tree->Branch("linearTime60", linearTime60, "linearTime60[4]/F");
  tree->Branch("fallingTime", fallingTime, "fallingTime[4]/F");
  tree->Branch("risetime", risetime, "risetime[4]/F");
  tree->Branch("constantThresholdTime", constantThresholdTime, "constantThresholdTime[4]/F");
  tree->Branch("isRinging", _isRinging, "isRinging[4]/O");
  
  for(int i = 0; i < 1000; i++){
    time[0][i] = 0.;
    time[1][i] = 0.;
    for(int iCh = 0; iCh < 4; iCh++){
      channel[iCh][i] = 0.;
    }
  }
  
  // Event loop
  for(waveformEvent.eventId = iEvent; waveformEvent.eventId < iEvent + nEvents;
      waveformEvent.eventId++) {
    hdf5io_read_event(waveformFile, &waveformEvent);
    
    event = waveformEvent.eventId;
    
    for(i = 0; i < NSAMPLES; i++) {
      if(i < waveformFile->nPt)
	time[0][i] = waveformAttr.dt*(i%frameSize);
      else
	time[0][i] = 0.;
      //printf("%24.16e ", waveformAttr.dt*(i%frameSize));
      j = 0;
      for(iCh=0; iCh<SCOPE_NCH; iCh++) {
	if((1<<iCh) & waveformAttr.chMask) {
	  if(i < waveformFile->nPt){
	    channel[iCh][i] = ((waveformBuf[j * waveformFile->nPt + i]
			       - waveformAttr.yoff[iCh])
	      * waveformAttr.ymult[iCh]
	      + waveformAttr.yzero[iCh]) / ADC;
	  } else {
	    channel[iCh][i] = 0.;
	  }
	  // printf("%24.16e ", (waveformBuf[j * waveformFile->nPt + i]
	  // 		      - waveformAttr.yoff[iCh])
	  // 	 * waveformAttr.ymult[iCh]
	  // 	 + waveformAttr.yzero[iCh]);
	  j++;
	}
      }
      // printf("\n");
      // if((i+1) % frameSize == 0)
      // 	printf("\n");
    }
    // printf("\n");

    // loop over channels for pulse processing
    for(iCh=0; iCh<SCOPE_NCH; iCh++) {
      if((1<<iCh) & waveformAttr.chMask) {
	xmin[iCh] = 0.;
	amp [iCh] = 0.;
	base[iCh] = 0.;
	integral[iCh] = 0.;
	integralFull[iCh] = 0.;
	gauspeak[iCh] = 0.;
	sigmoidTime[iCh] = 0.;
	linearTime0[iCh] = 0.;
	linearTime15[iCh] = 0.;
	linearTime30[iCh] = 0.;
	linearTime45[iCh] = 0.;
	linearTime60[iCh] = 0.;
	risetime[iCh] = 0.;
	constantThresholdTime[iCh] = 0.;
	
	// Make pulse shape graph
	TString pulseName = Form("pulse_event%d_ch%d", event, iCh);
	TGraphErrors* pulse = GetTGraph( channel[iCh], time[0] );

	// Estimate baseline
	float baseline;
	baseline = GetBaseline( pulse, 5 ,150, pulseName );
        base[iCh] = baseline;

	// Correct pulse shape for baseline offset + amp/att
	for(int j = 0; j < NSAMPLES; j++) {
 	  channel[iCh][j] = channel[iCh][j] + baseline;
	}

	int index_min = FindMinAbsolute(NSAMPLES, channel[iCh]);
	
	// Recreate the pulse TGraph using baseline-subtracted channel data
	delete pulse;
	pulse = GetTGraph( channel[iCh], time[0] );

	xmin[iCh] = index_min;

	Double_t tmpAmp = 0.0;
	Double_t tmpMin = 0.0;
	pulse->GetPoint(index_min, tmpMin, tmpAmp);
	amp[iCh] = tmpAmp * ADC;

	// Get pulse integral
	if ( xmin[iCh] != 0 ) {
	  integral[iCh] = GetPulseIntegral( index_min, 20, channel[iCh], time[0] );
	  integralFull[iCh] = GetPulseIntegral( index_min , channel[iCh], "full");
        }
	else {
	  integral[iCh] = 0.0;
	  integralFull[iCh] = 0.0;
        }

	// Gaussian time stamp and constant-fraction fit
	Double_t min = 0.; Double_t low_edge = 0.; Double_t high_edge = 0.; Double_t y = 0.; 
	pulse->GetPoint(index_min, min, y);	
	pulse->GetPoint(index_min-4, low_edge, y); // time of the low edge of the fit range
	pulse->GetPoint(index_min+4, high_edge, y);  // time of the upper edge of the fit range

	float timepeak   = 0;
        float fs[6]; // constant-fraction fit output
	float fs_falling[6]; // falling exp timestapms
	float cft_low_range  = 0.03;
	float cft_high_range = 0.20;
       
	if(xmin[iCh] != 0.0) {
	  timepeak =  GausFit_MeanTime(pulse, low_edge, high_edge);
	  RisingEdgeFitTime( pulse, index_min, cft_low_range, cft_high_range, fs, event, "linearFit_" + pulseName, false );
	}

	_isRinging[iCh] = isRinging( index_min, channel[iCh] );
        // for output tree
	gauspeak[iCh] = timepeak;
	risetime[iCh] = fs[0];
	linearTime0[iCh] = fs[1];
	linearTime15[iCh] = fs[2];
	linearTime30[iCh] = fs[3];
	linearTime45[iCh] = fs[4];
	linearTime60[iCh] = fs[5];
	fallingTime[iCh] = fs_falling[0];
	constantThresholdTime[iCh] = ConstantThresholdTime( pulse, 75);

	FFTtime[iCh] = FFT_MeanTime(time[0], channel[iCh], NSAMPLES);
	
	// reconvert to "mV" with ADC factor
	for(int s = 0; s < NSAMPLES; s++)
	  channel[iCh][s] *= ADC;
	
	delete pulse;
	 
        
	
      }
    }

    
    tree->Fill();
  }

  ofile->cd();
  tree->Write();
  ofile->Close();
  
  free(waveformBuf);
  hdf5io_close_file(waveformFile);
    
  return EXIT_SUCCESS;
}
