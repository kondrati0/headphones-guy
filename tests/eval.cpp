// Score the tracker against reference beat times from a neural beat tracker.
// Needs NAME.f32 (mono float32, 48 kHz) and NAME.beats (one beat time in seconds per line).
//   ffmpeg -i track.mp3 -ac 1 -ar 48000 -f f32le NAME.f32
//   g++ -O2 -std=c++17 -I../src eval.cpp -o eval && ./eval NAME [verbose]
#ifndef TRACKER
#define TRACKER "tracker.h"
#endif
#include TRACKER
#include <cstdio>
#include <string>
#include <vector>
int main(int argc,char**argv){
  int verbose = argc>2;
  std::string nm=argv[1];
  FILE*f=fopen((nm+".f32").c_str(),"rb"); std::vector<float> s; float buf[48000]; size_t n;
  while((n=fread(buf,4,48000,f))>0) s.insert(s.end(),buf,buf+n); fclose(f);
  std::vector<double> rb; FILE*g=fopen((nm+".beats").c_str(),"r"); double x; while(fscanf(g,"%lf",&x)==1) rb.push_back(x); fclose(g);
  BeatTracker tr; tr.init(48000);
  double clock=0; size_t bi=0; int tot=0,nod=0,hit=0,off=0,dbl=0,half=0; double esum=0; int ecount=0;
  int lastMode=-1; double bpmPrint=-1;
  for(size_t i=0;i+480<=s.size();i+=480){
    tr.feed(&s[i],480,clock); clock+=0.01;
    GuyStatus st=tr.status(clock);
    if(verbose && (st.mode!=lastMode || fabs(st.bpm-bpmPrint)>1.0)){ printf("  t=%6.2f mode=%d bpm=%6.1f conf=%.2f cand=%.1f\n",clock,st.mode,st.bpm,tr.lastConf,tr.lastCandBpm); lastMode=st.mode; bpmPrint=st.bpm;}
    while(bi<rb.size() && rb[bi]<=clock-0.02){
      double tb=rb[bi++]; tot++;
      if(st.mode!=MODE_NOD) continue;
      nod++;
      double k=floor((tb-st.gridT0)/st.period+0.5), e=tb-(st.gridT0+k*st.period);
      if(fabs(e)<0.035) {hit++; esum+=e; ecount++;}
      else if(fabs(fabs(e)-st.period/2)<0.035) off++;
      double refT = bi<rb.size()? rb[bi]-tb : st.period;
      double r=st.period/refT; if(r<0.6) dbl++; else if(r>1.6) half++;
    }
  }
  printf("%-14s refBeats %4d  nodding %5.1f%%  hit %5.1f%% of nodding  offbeat %4.1f%%  double %4.1f%% half %4.1f%%  meanSignedErr %5.1fms  finalBpm %.2f\n",
    nm.c_str(), tot, 100.0*nod/tot, nod?100.0*hit/nod:0, nod?100.0*off/nod:0, nod?100.0*dbl/nod:0, nod?100.0*half/nod:0, ecount?1000*esum/ecount:0, tr.status(clock).bpm);
}
