#pragma once
// JSON estrito, limitado e sem dependência externa. UTF-8 é preservado.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rc {
struct Json {
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v = nullptr;
    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool x):v(x){}
    Json(int x):v(double(x)){}
    Json(double x):v(x){}
    Json(const char* x):v(std::string(x)){}
    Json(std::string x):v(std::move(x)){}
    Json(Array x):v(std::move(x)){}
    Json(Object x):v(std::move(x)){}
    bool null()const{return std::holds_alternative<std::nullptr_t>(v);}
    bool object()const{return std::holds_alternative<Object>(v);}
    bool array()const{return std::holds_alternative<Array>(v);}
    bool has(const std::string& k)const{return object()&&std::get<Object>(v).count(k);}
    const Json& operator[](const std::string& k)const {
        static const Json nil;
        if(!object())return nil;
        auto i=std::get<Object>(v).find(k);
        return i==std::get<Object>(v).end()?nil:i->second;
    }
    Json& operator[](const std::string& k){if(!object())v=Object{};return std::get<Object>(v)[k];}
    const Array& items()const {static const Array empty;return array()?std::get<Array>(v):empty;}
    const Object& fields()const {static const Object empty;return object()?std::get<Object>(v):empty;}
    double number(double fallback=0)const {
        if(auto p=std::get_if<double>(&v))return *p;
        if(auto p=std::get_if<std::string>(&v)) {
            char* end=nullptr;double x=std::strtod(p->c_str(),&end);
            if(end!=p->c_str()&&*end=='\0'&&std::isfinite(x))return x;
        }
        return fallback;
    }
    int integer(int fallback=-1)const {
        double n=number(double(fallback));
        return n>=-2147483648.0&&n<=2147483647.0&&std::floor(n)==n?int(n):fallback;
    }
    bool boolean(bool fallback=false)const {
        auto p=std::get_if<bool>(&v);return p?*p:fallback;
    }
    std::string text(const std::string& fallback="")const {
        if(auto p=std::get_if<std::string>(&v))return *p;
        if(auto p=std::get_if<double>(&v)){char b[64];std::snprintf(b,sizeof b,"%.17g",*p);return b;}
        return fallback;
    }
    bool operator==(const Json& other)const{return v==other.v;}
    bool operator!=(const Json& other)const{return v!=other.v;}
    static Json parse(std::string_view input);
    std::string dump()const;
};

class JsonParser {
    std::string_view s;size_t p=0;
    [[noreturn]] void fail()const{throw std::runtime_error("JSON inválido na posição "+std::to_string(p));}
    void ws(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
    bool take(char c){if(p<s.size()&&s[p]==c){++p;return true;}return false;}
    uint32_t hex4(){uint32_t u=0;for(int i=0;i<4;++i){if(p>=s.size())fail();char c=s[p++];u<<=4;
        if(c>='0'&&c<='9')u+=c-'0';else if(c>='a'&&c<='f')u+=c-'a'+10;
        else if(c>='A'&&c<='F')u+=c-'A'+10;else fail();}return u;}
    static void utf8(std::string& out,uint32_t u){
        if(u<0x80)out+=char(u);
        else if(u<0x800){out+=char(0xc0|(u>>6));out+=char(0x80|(u&63));}
        else if(u<0x10000){out+=char(0xe0|(u>>12));out+=char(0x80|((u>>6)&63));out+=char(0x80|(u&63));}
        else{out+=char(0xf0|(u>>18));out+=char(0x80|((u>>12)&63));out+=char(0x80|((u>>6)&63));out+=char(0x80|(u&63));}
    }
    std::string str(){
        if(!take('"'))fail();std::string out;
        while(p<s.size()){
            unsigned char c=s[p++];if(c=='"')return out;if(c<32)fail();
            if(c!='\\'){out+=char(c);continue;}if(p>=s.size())fail();
            char e=s[p++];switch(e){
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;
                case 'r':out+='\r';break;case 't':out+='\t';break;
                case 'u':{uint32_t u=hex4();if(u>=0xd800&&u<=0xdbff){
                    if(!take('\\')||!take('u'))fail();uint32_t l=hex4();if(l<0xdc00||l>0xdfff)fail();
                    u=0x10000+((u-0xd800)<<10)+(l-0xdc00);
                }else if(u>=0xdc00&&u<=0xdfff)fail();utf8(out,u);break;}
                default:fail();
            }
        }fail();
    }
    Json val(int depth){
        if(depth>64)fail();ws();if(p>=s.size())fail();
        if(s[p]=='"')return Json(str());
        if(take('{')){Json::Object o;ws();if(take('}'))return o;
            do{ws();std::string k=str();ws();if(!take(':'))fail();o[std::move(k)]=val(depth+1);ws();if(take('}'))return o;}while(take(','));fail();}
        if(take('[')){Json::Array a;ws();if(take(']'))return a;
            do{a.push_back(val(depth+1));ws();if(take(']'))return a;}while(take(','));fail();}
        for(auto token:{std::string_view("true"),std::string_view("false"),std::string_view("null")})
            if(s.substr(p,token.size())==token){p+=token.size();if(token=="null")return Json();return Json(token=="true");}
        size_t start=p;take('-');if(!take('0')){if(p>=s.size()||s[p]<'1'||s[p]>'9')fail();while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
        if(take('.')){size_t b=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==b)fail();}
        if(take('e')||take('E')){if(!take('+'))take('-');size_t b=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==b)fail();}
        std::string raw(s.substr(start,p-start));char* end=nullptr;double x=std::strtod(raw.c_str(),&end);
        if(!end||*end||!std::isfinite(x))fail();return x;
    }
public:
    explicit JsonParser(std::string_view in):s(in){}
    Json parse(){Json r=val(0);ws();if(p!=s.size())fail();return r;}
};
inline Json Json::parse(std::string_view s){return JsonParser(s).parse();}
inline std::string quote(const std::string& s){
    std::string o="\"";for(unsigned char c:s){switch(c){
        case '"':o+="\\\"";break;case '\\':o+="\\\\";break;case '\n':o+="\\n";break;
        case '\r':o+="\\r";break;case '\t':o+="\\t";break;
        default:if(c<32){char b[8];std::snprintf(b,sizeof b,"\\u%04x",c);o+=b;}else o+=char(c);
    }}return o+'"';
}
inline std::string Json::dump()const {
    if(null())return "null";
    if(auto p=std::get_if<bool>(&v))return *p?"true":"false";
    if(auto p=std::get_if<double>(&v)){if(!std::isfinite(*p))throw std::runtime_error("Número JSON não finito");return text();}
    if(auto p=std::get_if<std::string>(&v))return quote(*p);
    std::string o=array()?"[":"{";bool first=true;
    if(array())for(const auto& x:items()){if(!first)o+=',';first=false;o+=x.dump();}
    else for(const auto& kv:fields()){if(!first)o+=',';first=false;o+=quote(kv.first)+':'+kv.second.dump();}
    return o+(array()?']':'}');
}

// O pw-dump -m emite arrays JSON consecutivos. Processar objeto por objeto
// evita manter o dump inteiro e seus parâmetros de vídeo na memória.
class JsonStream {
    bool array_=false,in_string_=false,escape_=false,expect_value_=true,had_value_=false;
    int depth_=0;std::string object_;
public:
    size_t batches=0,objects=0;
    void reset(){array_=in_string_=escape_=false;expect_value_=true;had_value_=false;depth_=0;object_.clear();batches=objects=0;}
    bool empty()const{return !array_&&object_.empty();}
    void feed(std::string_view chunk,const std::function<void(const Json&)>& emit){
        for(char c:chunk){
            if(depth_){
                object_+=c;if(object_.size()>8*1024*1024)throw std::runtime_error("Objeto PipeWire excede 8 MiB");
                if(in_string_){if(escape_)escape_=false;else if(c=='\\')escape_=true;else if(c=='"')in_string_=false;}
                else if(c=='"')in_string_=true;
                else if(c=='{'||c=='['){if(++depth_>64)throw std::runtime_error("PipeWire: JSON muito profundo");}
                else if(c=='}'||c==']'){
                    if(--depth_==0){Json j=Json::parse(object_);emit(j);++objects;object_.clear();
                        if(object_.capacity()>131072)std::string().swap(object_);expect_value_=false;had_value_=true;}
                }continue;
            }
            if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;
            if(!array_){if(c!='[')throw std::runtime_error("PipeWire não retornou uma lista JSON");array_=true;expect_value_=true;had_value_=false;continue;}
            if(c==']'&&(!expect_value_||!had_value_)){array_=false;++batches;continue;}
            if(c==','&&!expect_value_){expect_value_=true;continue;}
            if(c=='{'&&expect_value_){depth_=1;object_="{";continue;}
            throw std::runtime_error("Lista PipeWire inválida");
        }
    }
};
}
