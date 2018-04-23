//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2014 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "webhook/server/boost/request_handler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <functional>

#include "common/Common.hh"

#include "webhook/server/boost/mime_types.hpp"
#include "webhook/server/boost/reply.hpp"
#include "webhook/server/boost/request.hpp"

#include "webhook/common/QuickHTML.hh"

using namespace std;

namespace http {
namespace server {




request_handler::request_handler() {

  //Default "/" handler dumps out info about what handles are registered
  registerHook("/", [this] (const map<string, string> &args, stringstream &results) {
      dumpRegisteredHandles(results);
    });

  registerHook("/about", [this] (const map<string, string> &args, stringstream &results) {
      dumpAbout(results);
    });
  
  registerHook("/bootstraps", [this] (const map<string, string> &args, stringstream &results) {
      dumpBootstraps(results);
    });
  

  
}

void request_handler::handle_request(const request& req, reply& rep) {

  //  cout << "Request is method:"<<req.method<<"\n"
  //     << "              uri:"<<req.uri<<"\n"
  //     << "         versions:"<<req.http_version_major<<"/"<<req.http_version_minor<<"\n";
            
  
  // Decode url to path.
  string request_path;
  if (!url_decode(req.uri, request_path)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }
  //cout <<"Request path:"<<request_path<<"\n";
  
  // Request path must be absolute and not contain "..".
  if (request_path.empty() || request_path[0] != '/'
      || request_path.find("..") != string::npos) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  string extension; //What kind of result to make

  
  size_t px=request_path.find('&');
  string tag=request_path.substr(0,px);
  string args="";
  if(!((px==string::npos)||(px==request_path.size()))){
    args = request_path.substr(px+1);
  }

  map<string,string> arg_map = parseArgString(args);

  //cout<<"Tag is '"<<tag<<"' args are '"<<args<<"'\n";
  //for(auto &k_v : arg_map){
  //  cout <<"Key: "<<k_v.first<<" val: "<<k_v.second<<endl;
  //}


  cbs_mutex.lock();
  auto name_func = cbs.find(tag);
  if(name_func==cbs.end()){
    //TODO: Send error message
  } else {
    stringstream ss;
    name_func->second(arg_map, ss);
    rep.content = ss.str();
    extension="html";
  }
  cbs_mutex.unlock();
    
  // Fill out the reply to be sent to the client.
  rep.status = reply::ok;
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = std::to_string(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);
}

bool request_handler::url_decode(const string& in, string& out) {
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%') {
      if (i + 3 <= in.size()) {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value) {
          out += static_cast<char>(value);
          i += 2;
        } else {
          return false;
        }
      } else {
        return false;
      }
    } else if (in[i] == '+') {
      out += ' ';
    } else {
      out += in[i];
    }
  }
  return true;
}


int request_handler::registerHook(const string &name, webhook::cb_web_handler_t func){

  int rc=0;
  cbs_mutex.lock();
  auto fptr = cbs.find(name);
  if(fptr == cbs.end()){
    cbs[name] = func;
  } else {
    cout <<"WebHook: RegisterHandler detected double register of function '"<<name<<"'. Skipping.\n";
    rc=-1;
  }
  cbs_mutex.unlock();
  return rc;
  
}
int request_handler::updateHook(const string &name, webhook::cb_web_handler_t func){

  int rc=0;
  cbs_mutex.lock();
  //auto fptr = cbs.find(name);
  cbs[name] = func;
  cbs_mutex.unlock();
  return rc;
  
}


int request_handler::deregisterHook(const string &name){
  cbs_mutex.lock();
  int num=cbs.erase(name);
  cbs_mutex.unlock();
  return (num==0) ? -1 : 0;
}

void request_handler::dumpRegisteredHandles(stringstream &results){
  html::mkHeader(results, "WebHook");
  vector<string> links;
  for(auto &name_hdlr : cbs)
    links.push_back(html::mkLink(name_hdlr.first, name_hdlr.first));
  
  html::mkHeader(results, "WebHooks");
  html::mkText(results,"WebHooks",1); 
  html::mkText(results,"The following hooks are known to this application:");
  html::mkList(results, links);
  html::mkFooter(results);
}

void request_handler::dumpAbout(stringstream &results){
  html::mkHeader(results,"About WebHook");
  html::mkText(results,"About WebHook",1);
  html::mkText(results,R"(WebHook is a simple add-on that allows multiple 
software components in an application to share a simple network interface
for debugging and basic restful api operations.)");
  html::mkText(results, html::mkLink("See available services", "/"));
  html::mkFooter(results);
  
}
void request_handler::dumpBootstraps(stringstream &results){
  html::mkHeader(results,"Bootstraps");
  html::mkText(results,"Bootstraps",1);
  html::mkText(results,"The bootstrap list for this node is:");
  vector<string> bss = faodel::bootstrap::GetStartupOrder();
  html::mkList(results, bss);
  html::mkFooter(results);
  
}

pair<string,string> request_handler::splitString(const string &item, char delim){
  size_t p1 = item.find(delim);
  if(p1==string::npos) return pair<string,string>(item,"");
  if(p1==item.size())  return pair<string,string>(item.substr(0,p1),"");
  if(p1==0)            return pair<string,string>("",item.substr(p1+1));
  return pair<string,string>(item.substr(0,p1), item.substr(p1+1));
}
map<string,string> request_handler::parseArgString(const string &args){

  map<string,string> res_map;
  size_t start=0, end=0;
  while((end=args.find('&',start)) != string::npos){
    if(start!=end){      
      string item = args.substr(start, end-start);      
      pair<string,string> res = splitString(item, '=');
      res_map[res.first] = res.second;
    }
    start=end+1;
  }
  if(start!=args.size()){
    string item = args.substr(start);
    pair<string,string> res = splitString(item, '=');
    res_map[res.first] = res.second;
  }  
  return res_map;
}

} // namespace server
} // namespace http
