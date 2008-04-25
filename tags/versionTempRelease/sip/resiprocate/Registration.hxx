#if !defined(REGISTRATION_HXX)
#define REGISTRATION_HXX


#include "sip2/util/Timer.hxx"
namespace Vocal2
{

class SipMessage;


class Registration
{
   public: 
      void processRequest(SipMessage* msg);  
      void processResponse(SipMessage* msg);
      
   private:  
      UInt64 mTimeTillExpiration;
      
      typedef enum 
      {
         Invalid=0,
         Init,
         Acitve,
         Terminated
      } State;
      
      State mState;
};
 
}

#endif


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 */



