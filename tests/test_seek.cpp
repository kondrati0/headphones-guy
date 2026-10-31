// Seek test on a real track: jumps around inside the track and measures how fast the nod grid agrees again
// with a tracker that was cold-started at the destination.
//   ffmpeg -i track.mp3 -ac 1 -ar 48000 -f f32le NAME.f32
//   g++ -O2 -std=c++17 -I../src test_seek.cpp -o test_seek && ./test_seek NAME [gap_seconds]
#ifndef TRACKER
#define TRACKER "tracker.h"
#endif
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>
#include TRACKER
// reference: continuous run; record grid per 10 ms of content time
struct G { bool ok; double t0, T; };
static std::vector<float> load(const std::string& nm){ FILE*f=fopen((nm+".f32").c_str(),"rb"); std::vector<float> s; float b[48000]; size_t n; while((n=fread(b,4,48000,f))>0) s.insert(s.end(),b,b+n); fclose(f); return s; }
int main(int argc,char**argv){
  double gap = argc>2 ? atof(argv[2]) : 0;   // seconds of silence inserted at the seek
  std::string nm=argv[1]; auto s=load(nm); size_t H=480; int nfr=s.size()/H;
  std::vector<G> ref(nfr); { BeatTracker tr; tr.init(48000); double c=0; for(int i=0;i<nfr;i++){ tr.feed(&s[i*H],H,c); c+=0.01; GuyStatus st=tr.status(c); ref[i]={st.mode==MODE_NOD,st.gridT0,st.period}; } }
  double len=nfr*0.01; double tot=0; int cnt=0, fail=0;
  for(double from : {40.0, 70.0, 100.0}) for(double to : {from+35.0, from-25.0}) {
    if(to<10||to>len-20||from>len-10) continue;
    BeatTracker cold; cold.init(48000); double cc=0;
    BeatTracker tr; tr.init(48000); double c=0; int fi=(int)(from/0.01), ti=(int)(to/0.01);
    for(int i=0;i<fi;i++){ tr.feed(&s[i*H],H,c); c+=0.01; tr.status(c); }
    std::vector<float> z(H,0.f); for(int g=0; g<(int)(gap/0.01); g++){ tr.feed(z.data(),H,c); c+=0.01; tr.status(c); }
    double cs=c; double okSince=-1, rec=-1;
    for(int i=ti;i<ti+1500 && i<nfr;i++){ tr.feed(&s[i*H],H,c); c+=0.01; GuyStatus st=tr.status(c);
      cold.feed(&s[i*H],H,cc); cc+=0.01; GuyStatus cs2=cold.status(cc);
      G r{cs2.mode==MODE_NOD && (c-cs)>6, cs2.gridT0 + (c-cc), cs2.period}; double el=c-cs;
      // reference beat times in content time -> our clock: content time = (i+1)*0.01, our clock offset = c - (i+1)*0.01
      bool good=false;
      if(st.mode==MODE_NOD && r.ok && fabs(st.period/r.T-1)<0.03){ double rb=r.t0; double k=floor((st.gridT0-rb)/r.T+0.5); double e=st.gridT0-(rb+k*r.T); good=fabs(e)<0.03; }
      if(good){ if(okSince<0) okSince=el; if(el-okSince>=3 && rec<0) rec=okSince; } else okSince=-1;
    }
    if(rec<0){ fail++; printf("  %s seek %.0f->%.0f: not recovered in 15 s\n",nm.c_str(),from,to);} else { tot+=rec; cnt++; }
  }
  printf("%-13s gap %.2fs: mean recovery %.2f s over %d seeks, %d failed\n",nm.c_str(),gap,cnt?tot/cnt:0,cnt,fail);
}
