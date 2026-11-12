#include "chiptune.hpp"
#include <cmath>
#include <iostream>
int main(){maple::Chiptune a,b;double energy=0;float peak=0;for(int mode=0;mode<2;++mode)for(int i=0;i<22050*8;++i){float x=a.next(mode!=0),y=b.next(mode!=0);if(!std::isfinite(x)||x!=y||std::abs(x)>1.01f)return 1;energy+=x*x;peak=std::fmax(peak,std::abs(x));}if(energy<1)return 2;std::cout<<"PASS original chiptune: deterministic finite output, peak="<<peak<<", energy="<<energy<<"; no listening claim\n";}
