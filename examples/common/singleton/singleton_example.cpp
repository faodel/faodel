// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include "common/Common.hh"

#include "ServiceA.hh"
#include "ServiceB.hh"
#include "ServiceC.hh"


using namespace std;


string fn_no_components() {
  return "";
}

int main(int argc, char *argv[]) {


  service_c::RegisterBootstrap(); //Manually create/register entity


  //We need to start the services, but in this case we don't have
  //a top-level bootstrap that needs to be registered. We can either
  //declare the empty registration in a well-named function, or just
  //use an inline lambda.
  faodel::bootstrap::Start(faodel::Configuration(), []() { return"";} );
  //faodel::bootstrap::Start(faodel::Configuration(), fn_no_components );



  service_a::doOp("Foobar");

  service_b::ServiceB b1;
  service_b::ServiceB b2;
  b1.doOp("From b1");
  b2.doOp("From b2");

  service_c::doOp("Stuff");


  faodel::bootstrap::Finish();

  service_c::DeregisterBootstrap(); //Manually deallocates entity


  return 0;
}
