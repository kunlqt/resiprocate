#ifndef LIMPETIMPL_H
#define LIMPETIMPL_H

#include "SetupDlgImpl.hxx"
#include "Limpet.hxx"
#include "resiprocate/SipStack.hxx"
#include "resiprocate/Security.hxx"
#include "resiprocate/TuIM.hxx"

class SetupDlgImpl;


class LimpetImpl : public Limpet
{
      Q_OBJECT
      
   public:
      LimpetImpl( QWidget* parent = 0, const char* name = 0,
                  bool modal = false, WFlags f = 0 );
      
      virtual void logon();
      virtual void send();
      virtual void setup();
      virtual void OnBnClickedCheckOnline();
      virtual void addBuddy();
      
      void receivedPage( const resip::Data& msg,
                         const resip::Uri& from,
                         const resip::Data& signedBy,
                         const resip::Security::SignatureStatus sigStatus,
                         const bool wasEncryped  );
      
      void message(const QString& msg);
      virtual void process(void);
      void sendPage(QString text,QString destination);
      void setStatus(bool online, QString note);
      //QString ClimpApp::addBuddy(QString name);
      void presenseUpdate(const resip::Uri& uri, bool open, const resip::Data& status );
      
      
      bool encryp;
      bool sign;
      
      resip::SipStack* sipStack;
      resip::TuIM* tuIM;
      
      
   private:
      SetupDlgImpl* setUpDlgImpl;
};

#endif // LIMPETIMPL_H


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 */
