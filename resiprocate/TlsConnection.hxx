#if !defined(TlsConnection_hxx)
#define TlsConnection_hxx

#include "resiprocate/Connection.hxx"
#include "resiprocate/os/HeapInstanceCounter.hxx"

#ifdef USE_SSL
#include <openssl/ssl.h>
#else
typedef void BIO;
typedef void SSL;
#endif

namespace resip
{

class Tuple;
class Security;

class TlsConnection : public Connection
{
   public:
      RESIP_HeapCount(TlsConnection);
      TlsConnection( const Tuple& who, Socket fd, Security* security, bool server=false );
      
      int read( char* buf, const int count );
      int write( const char* buf, const int count );
      virtual bool hasDataToRead(); // has data that can be read 
      virtual bool isGood(); // has valid connection
      
      Data peerName();
      
      typedef enum State { Broken, Accepting, Connecting, Handshaking, Up } State;
      static const char * fromState(State);
   private:
       State mState;

    State checkState();


     SSL* mSsl;
     BIO* mBio;
};
 
}


#endif
