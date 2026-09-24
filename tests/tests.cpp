#include "luna/processor.hpp"
#include "luna/audio.hpp"
#include "luna/wav.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
extern "C" {
#include "rnnoise.h"
}

namespace {
void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
template<class F> void fails(F f) { bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Expected rejection"); }
class DelayEngine final:public luna::NoiseSuppressionEngine {
    std::vector<float> memory_;std::size_t pos_=0,frame_;
public:
    DelayEngine(std::size_t frame=480,std::size_t delay=480):memory_(delay),frame_(frame){}
    const char* name()const noexcept override{return "test delay";}
    unsigned sampleRate()const noexcept override{return 48000;}
    std::size_t frameSize()const noexcept override{return frame_;}
    std::size_t latencySamples()const noexcept override{return memory_.size();}
    void process(const float* in,float* out)noexcept override{
        for(std::size_t i=0;i<frame_;++i){if(memory_.empty())out[i]=in[i];else{out[i]=memory_[pos_];memory_[pos_]=in[i];pos_=(pos_+1)%memory_.size();}}
    }
};
std::vector<float> noise(std::size_t n) {
    std::mt19937 random(2026);std::uniform_real_distribution<float> dist(-0.2f,0.2f);
    std::vector<float> v(n);for(auto&x:v)x=dist(random);return v;
}
std::vector<float> stream(luna::AudioProcessor& p,const std::vector<float>& input,std::size_t chunk) {
    std::vector<float> padded=input;padded.resize(input.size()+p.latencySamples());
    std::vector<float> out(padded.size());
    for(std::size_t i=0;i<padded.size();i+=chunk)p.process(padded.data()+i,out.data()+i,std::min(chunk,padded.size()-i));
    return {out.begin()+static_cast<std::ptrdiff_t>(p.latencySamples()),out.end()};
}
std::vector<float> read(const std::filesystem::path& path) {
    ma_decoder decoder;auto cfg=ma_decoder_config_init(ma_format_f32,1,48000);
#ifdef _WIN32
    auto result=ma_decoder_init_file_w(path.c_str(),&cfg,&decoder);
#else
    auto result=ma_decoder_init_file(path.c_str(),&cfg,&decoder);
#endif
    require(result==MA_SUCCESS,"Read test WAV failed");std::vector<float> out;std::array<float,4096> block{};
    for(;;){ma_uint64 n=0;auto code=ma_decoder_read_pcm_frames(&decoder,block.data(),block.size(),&n);require(code==MA_SUCCESS||code==MA_AT_END,"Decode failed");if(!n)break;out.insert(out.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(n));}
    ma_decoder_uninit(&decoder);return out;
}
void rawWav(const std::filesystem::path& path,unsigned rate,unsigned channels,const std::vector<short>& data){
    std::ofstream f(path,std::ios::binary);
    auto put=[&](unsigned x,unsigned bytes){for(unsigned i=0;i<bytes;++i)f.put(static_cast<char>((x>>(8*i))&255));};
    f.write("RIFF",4);put(36+static_cast<unsigned>(data.size()*2),4);f.write("WAVEfmt ",8);put(16,4);put(1,2);put(channels,2);
    put(rate,4);put(rate*channels*2,4);put(channels*2,2);put(16,2);f.write("data",4);put(static_cast<unsigned>(data.size()*2),4);
    for(auto x:data)put(static_cast<unsigned short>(x),2);
}
}
int main(){
    auto dir=std::filesystem::temp_directory_path()/("luna-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    unsigned passed=0,failed=0;
    auto test=[&](const char*name,std::function<void()>body){try{body();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("Virtual cable mode passes processed audio; mute and headphone modes do not",[]{
        luna::MonitoringOutput output;
        std::array<float,480> frame{};
        output.setMode(luna::MonitoringOutput::Mode::route);
        frame.fill(0.5f);output.render(frame.data(),frame.size());
        require(std::abs(frame.back()-0.5f)<1e-6f,"Cable output attenuated");
        output.setMode(luna::MonitoringOutput::Mode::headphones);
        frame.fill(0.5f);output.render(frame.data(),frame.size());
        require(std::abs(frame.back()-0.125f)<1e-6f,"Headphone output gain incorrect");
        output.setMode(luna::MonitoringOutput::Mode::muted);
        frame.fill(0.5f);output.render(frame.data(),frame.size());
        require(frame.back()==0,"Mute did not stop playback");
    });
    test("RNNoise silence, finite output and metrics",[]{
        luna::AudioProcessor p;std::array<float,480> in{},out{};
        for(int n=0;n<100;++n){p.process(in.data(),out.data(),480);for(auto x:out)require(std::isfinite(x)&&std::abs(x)<1e-6,"Silence changed");}
        auto m=p.metrics().snapshot();require(m.frames==100&&m.inputDb==-120&&m.outputDb==-120,"Incorrect metrics");
    });
    test("Wrapper equals upstream RNNoise with PCM scale conversion",[]{
        auto e=luna::makeRNNoise();auto* raw=rnnoise_create();require(raw!=nullptr,"Raw allocation");
        auto input=noise(480);std::array<float,480> scaled{},actual{},expected{};
        for(int frame=0;frame<20;++frame){for(std::size_t i=0;i<480;++i)scaled[i]=input[i]*32768;e->process(input.data(),actual.data());rnnoise_process_frame(raw,expected.data(),scaled.data());for(std::size_t i=0;i<480;++i)require(std::abs(actual[i]-std::clamp(expected[i]/32768,-1.0f,1.0f))<1e-6,"Scale/parity failure");}rnnoise_destroy(raw);
    });
    test("Bypass preserves signal and EOF tail with arbitrary chunks",[]{
        auto input=noise(12347);input.back()=0.9f;
        for(auto chunk:{1u,127u,480u,513u,2048u}){luna::AudioProcessor p;p.setEnabled(false);auto out=stream(p,input,chunk);require(out==input,"Bypass lost alignment/tail");}
    });
    test("Dry/wet alignment for pluggable engine frame and latency",[]{
        auto in=noise(937);
        for(float mix:{0.0f,0.25f,0.5f,1.0f}){luna::AudioProcessor p(std::make_unique<DelayEngine>(37,53));p.setStrength(mix);auto out=stream(p,in,71);for(std::size_t i=0;i<in.size();++i)require(std::abs(out[i]-in[i])<1e-7,"Dry/wet misalignment");}
    });
    test("RNNoise result independent of callback chunk partition",[]{
        auto in=noise(27001);luna::AudioProcessor a,b;auto x=stream(a,in,480),y=stream(b,in,193);require(x==y,"Chunking changed audio");
    });
    test("Invalid samples sanitized; input bounded; in-place allowed",[]{
        luna::AudioProcessor p;p.setEnabled(false);std::vector<float> x(2000,2.0f);x[0]=std::numeric_limits<float>::quiet_NaN();x[1]=std::numeric_limits<float>::infinity();p.process(x.data(),x.data(),x.size());for(auto v:x)require(std::isfinite(v)&&std::abs(v)<=1,"Unbounded output");require(p.metrics().snapshot().invalidSamples==2,"Invalid count");
    });
    test("Live control transitions remain finite and bounded",[]{
        luna::AudioProcessor p;auto x=noise(480);std::array<float,480> out{};for(int i=0;i<100;++i){p.setEnabled(i%2==0);p.setStrength(float(i%5)/4);p.process(x.data(),out.data(),480);for(auto v:out)require(std::isfinite(v)&&std::abs(v)<=1,"Bad transition");}
    });
    test("Synthetic stationary noise energy reduced (not speech quality)",[]{
        auto in=noise(48000*3);luna::AudioProcessor p;auto out=stream(p,in,480);double a=0,b=0;for(std::size_t i=48000;i<in.size();++i){a+=double(in[i])*in[i];b+=double(out[i])*out[i];}require(b<a*0.8,"Expected synthetic noise attenuation");
    });
    test("WAV bypass lengths, short files, quantization and trailing sample",[&]{
        for(std::size_t size:{1u,17u,479u,480u,481u,1001u,48000u}){auto input=noise(size);input.back()=0.7f;auto source=dir/(std::to_string(size)+".wav"),dest=dir/(std::to_string(size)+"-out.wav");{luna::WavWriter w(source);w.write(input.data(),input.size());w.finish();}auto r=luna::processWav(source,dest,1,false);auto a=read(source),b=read(dest);require(r.pipelineDelaySamples==960,"Incorrect RNNoise pipeline delay");require(r.samples==size&&a.size()==size&&b.size()==size,"WAV length mismatch");for(std::size_t i=0;i<size;++i)require(std::abs(a[i]-b[i])<=2.0/32768,"Bypass WAV changed");}
    });
    test("WAV 44.1 kHz stereo conversion to 48 kHz mono",[&]{
        auto source=dir/"stereo441.wav",dest=dir/"stereo-out.wav";std::vector<short> data(44100*2);
        for(std::size_t i=0;i<44100;++i){data[2*i]=static_cast<short>(8000*std::sin(i*0.02));data[2*i+1]=data[2*i];}rawWav(source,44100,2,data);auto result=luna::processWav(source,dest);auto out=read(dest);require(result.samples>=47998&&result.samples<=48002,"Resample duration");require(out.size()==result.samples,"Converted length");for(auto x:out)require(std::isfinite(x),"Nonfinite resample");
    });
    test("Originals and existing outputs protected, invalid inputs rejected",[&]{
        auto source=dir/"fixture.wav",dest=dir/"fixture-out.wav";luna::generateFixture(source,0.1);auto before=read(source);
        fails([&]{luna::processWav(source,source);});require(read(source)==before,"Original overwritten");
        luna::processWav(source,dest);auto after=read(dest);fails([&]{luna::processWav(source,dest);});require(read(dest)==after,"Existing output overwritten");
        fails([&]{luna::processWav(dir/"absent.wav",dir/"absent-out.wav");});require(!std::filesystem::exists(dir/"absent-out.wav"),"Unexpected output");
        auto corrupt=dir/"bad.wav";{std::ofstream f(corrupt);f<<"not a wav";}fails([&]{luna::processWav(corrupt,dir/"bad-out.wav");});
        auto truncated=dir/"truncated.wav";std::filesystem::copy_file(source,truncated);std::filesystem::resize_file(truncated,55);fails([&]{luna::processWav(truncated,dir/"truncated-out.wav");});
        auto empty=dir/"empty.wav";{luna::WavWriter w(empty);w.finish();}fails([&]{luna::processWav(empty,dir/"empty-out.wav");});require(!std::filesystem::exists(dir/"empty-out.wav"),"Partial output left behind");
    });
    test("Invalid engine/fixture/strength rejected",[&]{
        fails([]{luna::AudioProcessor p(nullptr);});fails([]{luna::AudioProcessor p(std::make_unique<DelayEngine>(0,0));});
        fails([&]{luna::generateFixture(dir/"invalid.wav",-1);});fails([&]{luna::processWav(dir/"fixture.wav",dir/"invalid.wav",2);});
        fails([&]{luna::makeDeepFilterNet3(dir/"missing.dll",dir/"DeepFilterNet3_onnx.tar.gz");});
    });
    std::filesystem::remove_all(dir);
    std::cout<<passed<<" test groups passed; "<<failed<<" failed\n";
    return failed?1:0;
}
