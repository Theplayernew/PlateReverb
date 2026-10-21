#include "../Source/PlateReverb.h"
#include <cstdio>
int main(){
  for (double sr : {44100.0, 48000.0, 96000.0}) {
    plate::PlateReverb r; r.prepare(sr);
    plate::Params p; p.mix=1.0f; p.decay=0.7f; r.setParams(p);
    float peak=0, e0=0, e1=0, e2=0; bool bad=false;
    int N=(int)(sr*6);
    for(int i=0;i<N;i++){
      float l,rr; float in = i==0?1.0f:0.0f;
      r.process(in,in,l,rr);
      if(!std::isfinite(l)||!std::isfinite(rr)) bad=true;
      float e=l*l+rr*rr; peak=std::max(peak,std::sqrt(e));
      if(i>0.1*sr&&i<0.6*sr)e0+=e; else if(i>1.5*sr&&i<2.0*sr)e1+=e; else if(i>4*sr&&i<4.5*sr)e2+=e;
    }
    printf("sr=%.0f peak=%.3f nan=%d  E(0.1-0.6s)=%.2e E(1.5-2s)=%.2e E(4-4.5s)=%.2e\n",sr,peak,bad,e0,e1,e2);
  }
  // max decay stability
  plate::PlateReverb r; r.prepare(48000); plate::Params p; p.mix=1; p.decay=1; p.modDepth=1; r.setParams(p);
  float mx=0; for(int i=0;i<48000*30;i++){float l,rr; float in=(i<48000)?((i*7919%1000)/500.f-1.f):0; r.process(in,in,l,rr); mx=std::max(mx,std::fabs(l));}
  printf("max-decay peak out=%.3f\n",mx);
}
