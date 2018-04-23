// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <assert.h>
#include <string.h>

#include "lunasa/core/LunasaCoreBase.hh"

using namespace std;

namespace lunasa {
namespace internal {

LunasaCoreBase::LunasaCoreBase(std::string subcomponent_name)
  : faodel::LoggingInterface("lunasa", subcomponent_name) 
{
  configured=false;
}

LunasaCoreBase::~LunasaCoreBase()
{}

void LunasaCoreBase::init(const faodel::Configuration &config)
{
  assert(!configured && "Attempted to Init() LunasaCore multiple times");
  
  string lmm_name, emm_name, def_mm;
  bool use_webhook;
  config.GetLowercaseString(&lmm_name,        "lunasa.lazy_memory_manager",  "malloc");
  config.GetLowercaseString(&emm_name,        "lunasa.eager_memory_manager", "tcmalloc");
  config.GetLowercaseString(&def_mm,          "lunasa.default_mm_style",     "lazy");
  config.GetBool(&use_webhook,                "lunasa.use_webhook",          "true");

  ConfigureLogging(config); //Pull out any logging options

  dbg("New lunasacore "+GetType()+" initializing. LazyMem: "+lmm_name+" EagerMem: "+emm_name+" DefStyle: "+def_mm);

  init(lmm_name, emm_name, use_webhook, config);

  configured=true;
}

} // namespace internal
} // namespace lunasa
