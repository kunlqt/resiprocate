#include <cppunit/TextTestRunner.h>
#include <cppunit/TextTestResult.h>

#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Random.hxx"
#include "tfm/CommandLineParser.hxx"
#include "tfm/Fixture.hxx"
#include "tfm/RouteGuard.hxx"
#include "tfm/Sequence.hxx"
#include "tfm/SipEvent.hxx"
#include "tfm/TestProxy.hxx"
#include "tfm/TestUser.hxx"
#include "tfm/CheckFetchedContacts.hxx"
#include "tfm/predicates/ExpectUtils.hxx"

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::TEST

using namespace CppUnit;
using namespace resip;

static const int WaitFor100 = 1000;
static const int WaitFor180 = 1000;
static const int WaitFor487 = 1000;
static const int WaitForAck = 1000;  //immediate ACK for 4xx and CANCEL; not ACK for 200
static const int WaitForCommand = 1000;
static const int WaitForResponse = 1000;
static const int WaitForRegistration = 1000;
static const int PauseTime = 100;
static const int WaitForPause = 1100;
static const int WaitForEndOfTest = 1000;
static const int WaitForEndOfSeq = 1000;
static const int Seconds = 1000;

const Data transport("udp");
static NameAddr localhost;

class TestHolder : public Fixture
{
   public:
      
///***************************************** tests start here ********************************//
      void testRegisterBasic()
      {
         WarningLog(<<"*!testRegisterBasic!*");
         
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();
      }

      static boost::shared_ptr<SipMessage>
      largeCallId(boost::shared_ptr<SipMessage> msg)
      {
         const int oversize = 2048;
         Data callId(oversize, true);
         for (int i = 0; i < oversize/resip::Random::maxLength; ++i)
         {
            callId += resip::Random::getRandomHex(resip::Random::maxLength);
         }
         callId += resip::Random::getRandomHex(oversize - resip::Random::maxLength*(oversize/resip::Random::maxLength));
         msg->header(h_CallId).value() = callId;

         return msg;
      }

      void testOversizeCallIdRegister()
      {
         WarningLog(<<"*!testOversizeCallIdRegister!*");
         
         Seq(condition(largeCallId, jason->registerUser(60, jason->getDefaultContacts())),
             jason->expect(REGISTER/400, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();
      }

      void testRegisterClientRetransmits()
      {
         WarningLog(<<"*!testRegisterClientRetransmits!*");

         boost::shared_ptr<SipMessage> reg;
         Seq(save(reg, jason->registerUser(60, jason->getDefaultContacts())),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->retransmit(reg)),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();
      }

      void testInviteClientRetransmissionsWithRecovery()
      {
         WarningLog(<<"*!testInviteClientRetransmissionsWithRecovery!*");
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();

         
         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),
             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, derek->note("R1")),
                     jason->expect(INVITE, contact(derek), 1000, derek->note("R2")),
                     jason->expect(INVITE, contact(derek), 2000, chain(derek->note("R3"), jason->ring(), jason->answer())),
                     derek->expect(INVITE/180, from(jason), WaitFor100, derek->noAction()),
                     derek->expect(INVITE/200, contact(jason), WaitForResponse, derek->ack()),
                     jason->expect(ACK, from(derek), WaitForResponse, derek->noAction()))),
             WaitForEndOfTest);
         ExecuteSequences();  
      }

      void testInviteBasic()
      {
         WarningLog(<<"*!testInviteBasic!*");
         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForResponse, derek->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
         
         Seq(jason->invite(*derek),
             optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction())),
             jason->expect(INVITE/407, from(proxy), WaitForResponse, chain(jason->ack(), jason->digestRespond())),
             And(Sub(optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction()))),
                 Sub(derek->expect(INVITE, contact(jason), WaitForCommand, chain(derek->ring(), derek->answer())),
                     jason->expect(INVITE/180, from(derek), WaitFor100, jason->noAction()),
                     jason->expect(INVITE/200, contact(derek), WaitForResponse, jason->ack()),
                     derek->expect(ACK, from(jason), WaitForResponse, jason->noAction()))),
             WaitForEndOfTest);
         ExecuteSequences();  
      }

      static boost::shared_ptr<SipMessage>
      largeContact(boost::shared_ptr<SipMessage> msg)
      {
         assert(msg->exists(h_Contacts) &&
                !msg->header(h_Contacts).empty());

         const int oversize = 2048;
         Data contactUser(oversize, true);
         for (int i = 0; i < oversize/resip::Random::maxLength; ++i)
         {
            contactUser += resip::Random::getRandomHex(resip::Random::maxLength);
         }
         contactUser += resip::Random::getRandomHex(oversize - resip::Random::maxLength*(oversize/resip::Random::maxLength));
         msg->header(h_Contacts).front().uri().user() = contactUser;

         return msg;
      }

      void testOversizeContactRegister()
      {
         WarningLog(<<"*!testOversizeContactRegister!*");
         Seq(condition(largeContact, jason->registerUser(60, jason->getDefaultContacts())),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/500, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();
      }

      void testUnregisterAll()
      {
         WarningLog(<<"*!testUnregisterAll!*");
         
         NameAddr na;
		 set<NameAddr> all;
		 set<NameAddr> emptySet;
          
         na.setAllContacts();
         all.insert( na );

         Seq(jason1->registerUser(60, jason1->getDefaultContacts()),
             jason1->expect(REGISTER/407, from(proxy), WaitForResponse, jason1->digestRespond()),
             jason1->expect(REGISTER/200, from(proxy), WaitForResponse, new CheckFetchedContacts(jason1->getDefaultContacts() )),
             WaitForEndOfTest);
         ExecuteSequences();

         Seq(jason2->registerUser(60, jason2->getDefaultContacts()),
             jason2->expect(REGISTER/407, from(proxy), WaitForResponse, jason2->digestRespond()),
             jason2->expect(REGISTER/200, from(proxy), WaitForResponse, new CheckFetchedContacts( mergeContacts(*jason1, *jason2) )),
             WaitForEndOfTest);
         ExecuteSequences();

         Seq(jason1->registerUser(0, all ),
             jason1->expect(REGISTER/200, from(proxy), WaitForResponse, new CheckFetchedContacts(emptySet)),
             WaitForEndOfTest);
             
         ExecuteSequences();
      }

      void testInfo()
      {
         WarningLog(<<"*!testInfo!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();

         Seq(derek->info(jason),
             derek->expect(INFO/407, from(proxy), 1000, derek->digestRespond()),
             jason->expect(INFO, from(derek), 1000, jason->ok()),
             derek->expect(INFO/200, from(jason), 1000, derek->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
      }


      void testInviteClientRetransmitsAfter200()
      {
         WarningLog(<<"*!testInviteClientRetransmitsAfter200!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
         
         boost::shared_ptr<SipMessage> inv;
         boost::shared_ptr<SipMessage> ok;
         Seq(save(inv, derek->invite(*jason)),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),
             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, from(derek), 2000, chain(jason->send100(), jason->ring(), (ok <= jason->answer()))),
                     derek->expect(INVITE/180, from(jason), WaitFor100, derek->noAction()),
                     derek->expect(INVITE/200, from(jason), WaitForResponse, chain(derek->retransmit(inv), derek->pause(500), jason->retransmit(ok))),
                     derek->expect(INVITE/200, from(jason), 1000, derek->ack()),
                     jason->expect(ACK, from(derek), WaitForResponse, derek->noAction()))),
             WaitForEndOfTest);
         ExecuteSequences();  
      }


      void testNonInviteClientRetransmissionsWithRecovery()
      {
         WarningLog(<<"*!testNonInviteClientRetransmissionsWithRecovery!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();

         Seq(derek->info(jason),
             derek->expect(INFO/407, from(proxy), 1000, derek->digestRespond()),
             jason->expect(INFO, from(derek), 1000, jason->noAction()),
             jason->expect(INFO, from(derek), 1000, jason->ok()),
             derek->expect(INFO/200, from(jason), 1000, derek->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
      }

      void testNonInviteClientRetransmissionsWithTimeout()
      {
         WarningLog(<<"*!testNonInviteClientRetransmissionsWithTimeout!*");
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();

         Seq(derek->info(jason),
             derek->expect(INFO/407, from(proxy), 1000, derek->digestRespond()),
             jason->expect(INFO, from(derek), 1000, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             jason->expect(INFO, from(derek), 4800, jason->noAction()),
             // note: 408 to NIT are not forwarded by repro
             //derek->expect(INFO/408, from(proxy), 4800, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
      }
      
      void testNonInviteServerRetransmission()
      {
         WarningLog(<<"*!testNonInviteServerRetransmission!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();

         boost::shared_ptr<SipMessage> infoMsg;
         Seq(save(infoMsg, derek->info(jason)),
             derek->expect(INFO/407, from(proxy), 1000, derek->digestRespond()),
             jason->expect(INFO, from(derek), 1000, jason->noAction()),
             jason->expect(INFO, from(derek), 1000, jason->ok()),
             derek->expect(INFO/200, from(jason), 1000, derek->retransmit(infoMsg)),
             derek->expect(INFO/200, from(jason), 1000, derek->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
      }


      void testInviteTransportFailure()
      {
         WarningLog(<<"*!testInviteTransportFailure!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
         
         Seq(chain(jason->closeTransport(), derek->invite(*jason)),
             optional(derek->expect(INVITE/100, from(proxy), 300+WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(),
                                                                           derek->digestRespond())),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/503, from(proxy), WaitForResponse, derek->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();  
      }

      void testInviteNoDNS()
      {
         WarningLog(<<"*!testInviteNoDNS!*");
         
         RouteGuard dGuard(*proxy, "sip:.*@.*", "sip:foobar@dfkaslkfdas.com");
         Seq(jason->invite(*derek),
             optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction())),
             jason->expect(INVITE/407, from(proxy), WaitForResponse, chain(jason->ack(), jason->digestRespond())),
             optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction())),
             jason->expect(INVITE/503, from(proxy), 5000, jason->ack()),
             WaitForEndOfTest);
         ExecuteSequences();  
      }

      void testBasic302()
      {
         WarningLog(<<"*!testBasic302!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);
         Seq(david->registerUser(60, david->getDefaultContacts()),
             david->expect(REGISTER/407, from(proxy), WaitForResponse, david->digestRespond()),
             david->expect(REGISTER/200, from(proxy), WaitForResponse, david->noAction()),
             WaitForEndOfSeq);
         ExecuteSequences();
         
         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, jason->send302()),
                     And(Sub(jason->expect(ACK, from(proxy), WaitForResponse, jason->noAction())),
                         Sub(derek->expect(INVITE/302, from(jason), WaitForResponse, chain(derek->ack(), derek->invite(*david))),
                             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
                             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),
                             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                                 Sub(david->expect(INVITE, contact(derek), WaitForCommand, david->ok()),
                                     derek->expect(INVITE/200, contact(david), WaitForResponse, derek->ack()),
                                     david->expect(ACK, from(derek), WaitForCommand, david->noAction()))))))),
             WaitForEndOfTest);
         ExecuteSequences();  
      }

      void testInviteBusy()
      {
         WarningLog(<<"*!testInviteBusy!*");

         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForRegistration, derek->noAction()),
             WaitForEndOfTest);
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, chain(jason->send100(),jason->send486())),
                     jason->expect(ACK, from(proxy), WaitForAck, jason->noAction()),
                     derek->expect(INVITE/486, from(proxy), WaitForResponse, derek->ack()))),
             WaitForEndOfTest);
         
         ExecuteSequences();  
      }

#if 0
      void testInviteAllBusyContacts()
      {
         WarningLog(<<"*!testInviteAllBusyContacts!*");
         TestCompany company("Company", stripHost(resip::DnsUtil::getLocalHostName()), proxy->getProxyUrl().uri().port());
         TestExt ext101(company, "ext101");
         TestExt ext102(company, "ext102");
         TestExt ext103(company, "ext103");
         TestExt ext104(company, "ext104");
         TestExt ext105(company, "ext105");
         TestAccount acct1("acct1", transport, ext101, ext102, ext103, ext104, ext105);

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             WaitForEndOfTest);
         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForRegistration, derek->noAction()),
             WaitForEndOfTest);
         Seq(acct1[ext104].registerUser(60, acct1[ext104].getDefaultContacts()),
             acct1[ext104].expect(REGISTER/407, from(proxy), WaitForResponse, acct1[ext104].digestRespond()),
             acct1[ext104].expect(REGISTER/200, from(proxy), WaitForRegistration, acct1[ext104].noAction()),
             WaitForEndOfTest);
         Seq(acct1[ext105].registerUser(60, acct1[ext105].getDefaultContacts()),
             acct1[ext105].expect(REGISTER/407, from(proxy), WaitForResponse, acct1[ext105].digestRespond()),
             acct1[ext105].expect(REGISTER/200, from(proxy), WaitForRegistration, acct1[ext105].noAction()),
             WaitForEndOfTest);
         ExecuteSequences();

         DialPlanGuard dGuard(company, "sip:(.......)@.*", 1, "sip:$1@" + derek->getContact().uri().getAor(), 1);
         DialPlanGuard dGuard1(company, "sip:(.......)@.*", 3, "sip:$1@" + acct1[ext104].getContact().uri().getAor(), 1);
         DialPlanGuard dGuard2(company, "sip:(.......)@.*", 7, "sip:$1@" + acct1[ext105].getContact().uri().getAor(), 1);

         Seq(acct1[ext101].invite(*jason),
             optional(acct1[ext101].expect(INVITE/100, from(proxy), WaitFor100, acct1[ext101].noAction())),
             acct1[ext101].expect(INVITE/407, from(proxy), WaitForResponse, chain(acct1[ext101].ack(), acct1[ext101].digestRespond())),

             And(Sub(optional(acct1[ext101].expect(INVITE/100, from(proxy), WaitFor100, acct1[ext101].noAction()))),
                 Sub(derek->expect(INVITE, contact(acct1[ext101]), WaitForCommand, chain(jason->send100(),derek->send503())),
                     acct1[ext104].expect(INVITE, contact(acct1[ext102]), WaitForCommand, acct1[ext104].send503()),
                     acct1[ext105].expect(INVITE, contact(acct1[ext102]), WaitForCommand, acct1[ext105].send503()),
                     acct1[ext101].expect(INVITE/480, from(proxy), 3*WaitForCommand, acct1[ext101].ack()))),
             WaitForEndOfTest);
         
         ExecuteSequences();  
      }
#endif

      void testInviteCalleeHangsUp()
      {
         WarningLog(<<"*!testInviteCalleeHangsUp!*");

         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForRegistration, derek->noAction()),
             WaitForEndOfTest);
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             WaitForEndOfTest);
         
         ExecuteSequences();

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, chain(jason->ring(), jason->answer())),
                     derek->expect(INVITE/180, from(jason), WaitFor180, derek->noAction()),
                     derek->expect(INVITE/200, contact(jason), WaitForResponse, derek->ack()),
                     jason->expect(ACK, from(derek), WaitForResponse, chain(jason->pause(PauseTime), jason->bye())),
                     derek->expect(BYE, from(jason), WaitForPause, derek->ok()),
                     jason->expect(BYE/200, from(derek), WaitForResponse, jason->noAction()))),
             WaitForEndOfTest);    

         ExecuteSequences();  
      }


      void testInviteCallerHangsUp()
      {
         WarningLog(<<"*!testInviteCallerHangsUp!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForResponse, jason->noAction()),
             WaitForEndOfSeq);

         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForResponse, derek->noAction()),
             WaitForEndOfSeq);
         
         ExecuteSequences();
         
         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, chain(jason->ring(), jason->answer())),
                     derek->expect(INVITE/180, from(jason), WaitFor100, derek->noAction()),
                     derek->expect(INVITE/200, contact(jason), WaitForResponse, derek->ack()),
                     jason->expect(ACK, from(derek), WaitForResponse, chain(jason->pause(PauseTime), derek->bye(jason))),
                     jason->expect(BYE, from(derek), WaitForPause, jason->ok()),
                     derek->expect(BYE/200, from(jason), WaitForResponse, derek->noAction()))),
             WaitForEndOfTest);

         ExecuteSequences();  
      }

      void testInviteCallerCancels()
      {
         WarningLog(<<"*!testInviteCallerCancels!*");
         
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             1000);
         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForRegistration, derek->noAction()),
             1000);
         ExecuteSequences();

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, jason->ring()),
                     derek->expect(INVITE/180, from(jason), WaitFor180, derek->cancel()),
                             
                     And(Sub(jason->expect(CANCEL, from(proxy), WaitForCommand, chain(jason->ok(), jason->send487())),
                             And(Sub(jason->expect(ACK, from(proxy), WaitForAck, jason->noAction())),
                                 Sub(derek->expect(INVITE/487, from(jason), WaitFor487, derek->ack())))),
                         Sub(derek->expect(CANCEL/200, from(proxy), WaitForResponse, derek->noAction()))))),
             WaitForEndOfTest);    

         ExecuteSequences();  
      }

      /*
        INV ->
        <- 100 (cancels timer)
        CANCEL ->
        <- 200 (CANCEL)
        **END**
        */
      void testInviteCallerCancelsNo487()
      {
         WarningLog(<<"*!testInviteCallerCancelsNo487!*");
         
         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             1000);
         ExecuteSequences();

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             And(Sub(optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction()))),
                 Sub(jason->expect(INVITE, contact(derek), WaitForCommand, jason->ring()),
                     derek->expect(INVITE/180, from(jason), WaitFor180, derek->cancel()),
                     
                     And(Sub(jason->expect(CANCEL, from(proxy), WaitForCommand, jason->ok()),
                             derek->expect(INVITE/408, from(proxy), 90*1000, derek->ack())), // !jf! should be faster than this ?
                         Sub(derek->expect(CANCEL/200, from(proxy), WaitForResponse, derek->noAction()))))),
             WaitForEndOfTest);    
         
         ExecuteSequences();  
      }

      void testInviteNoAnswerCancel()
      {
         WarningLog(<<"*!testInviteNoAnswerCancel!*");

         Seq(jason->registerUser(60, jason->getDefaultContacts()),
             jason->expect(REGISTER/407, from(proxy), WaitForResponse, jason->digestRespond()),
             jason->expect(REGISTER/200, from(proxy), WaitForRegistration, jason->noAction()),
             WaitForEndOfTest);
         Seq(derek->registerUser(60, derek->getDefaultContacts()),
             derek->expect(REGISTER/407, from(proxy), WaitForResponse, derek->digestRespond()),
             derek->expect(REGISTER/200, from(proxy), WaitForRegistration, derek->noAction()),
             WaitForEndOfTest);
         ExecuteSequences();

         Seq(jason->invite(*derek),
             optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction())),
             jason->expect(INVITE/407, from(proxy), WaitForResponse, chain(jason->ack(),jason->digestRespond())),

             And(Sub(optional(jason->expect(INVITE/100, from(proxy), WaitFor100, jason->noAction()))),
                 Sub(derek->expect(INVITE, contact(jason), WaitForCommand, derek->ring()),
                             
                     jason->expect(INVITE/180, from(derek), WaitFor180, jason->cancel()),
                             
                     And(Sub(jason->expect(CANCEL/200, from(proxy), WaitForResponse, jason->noAction())),
                         Sub(derek->expect(CANCEL, from(proxy), WaitForCommand, chain(derek->ok(), derek->send487())))),
                             
                     And(Sub(derek->expect(ACK, from(proxy), WaitForAck, derek->noAction())),
                         Sub(jason->expect(INVITE/487, from(derek), WaitFor487, jason->ack()))))),
                  
             WaitForEndOfTest);    

         ExecuteSequences();  
      }

      void testInviteNotFound()
      {
         WarningLog(<<"*!testInviteNotFound!*");

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             optional(derek->expect(INVITE/100, from(proxy), WaitForResponse, derek->noAction())),
             derek->expect(INVITE/480, from(proxy), WaitForResponse, derek->ack()),
             WaitForEndOfTest);
         ExecuteSequences();
      }


      void testInviteNotFoundServerRetransmits()
      {
         WarningLog(<<"*!testInviteNotFoundServerRetransmits!*");

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/480, from(proxy), WaitForResponse, derek->noAction()),
             derek->expect(INVITE/480, from(proxy), 2*WaitForResponse, derek->noAction()),
             derek->expect(INVITE/480, from(proxy), 4*WaitForResponse, derek->ack()),
             WaitForEndOfTest);
         ExecuteSequences();

         Seq(derek->invite(*jason),
             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/407, from(proxy), WaitForResponse, chain(derek->ack(), derek->digestRespond())),

             optional(derek->expect(INVITE/100, from(proxy), WaitFor100, derek->noAction())),
             derek->expect(INVITE/480, from(proxy), WaitForResponse, derek->noAction()),
             derek->expect(INVITE/480, from(proxy), 2*WaitForResponse, derek->noAction()),
             derek->expect(INVITE/480, from(proxy), 4*WaitForResponse, derek->ack()),
             WaitForEndOfTest);
         ExecuteSequences();
      }



      // provisioning here(automatic cleanup)
      static void createStatic()
      {
      }
};

#define TEST(_method) \
   suiteOfTests->addTest(new CppUnit::TestCaller<TestHolder>(#_method, &TestHolder::_method))
class MyTestCase
{
   public:
      static CppUnit::Test* suite()
      {
         CppUnit::TestSuite *suiteOfTests = new CppUnit::TestSuite( "Suite1" );
#if 0
         TEST(testRegisterBasic);
         TEST(testRegisterClientRetransmits);
         TEST(testInviteBasic);
         TEST(testInviteClientRetransmissionsWithRecovery);
         TEST(testUnregisterAll);
         TEST(testInfo);
         TEST(testInviteClientRetransmitsAfter200);
         TEST(testNonInviteClientRetransmissionsWithRecovery);
         TEST(testNonInviteClientRetransmissionsWithTimeout);
         TEST(testNonInviteServerRetransmission);
         //TEST(testInviteTransportFailure);
         TEST(testInviteNoDNS);
         TEST(testBasic302);
         TEST(testInviteBusy);
         //TEST(testInviteAllBusyContacts);
         TEST(testInviteCalleeHangsUp);
         TEST(testInviteCallerHangsUp);
         TEST(testInviteCallerCancels);
         TEST(testInviteCallerCancelsNo487);
         TEST(testInviteNoAnswerCancel);
         TEST(testInviteNotFound);
         TEST(testInviteNotFoundServerRetransmits);
         
         //TEST(testOversizeCallIdRegister);
         //TEST(testOversizeContactRegister);
#else
         TEST(testInviteBusy);
         //TEST(testInviteAllBusyContacts);
         TEST(testInviteCalleeHangsUp);
         TEST(testInviteCallerHangsUp);
         TEST(testInviteCallerCancels);
         TEST(testInviteCallerCancelsNo487);
         TEST(testInviteNoAnswerCancel);
         TEST(testInviteNotFound);
         TEST(testInviteNotFoundServerRetransmits);
#endif         
         return suiteOfTests;
      }
};

int main(int argc, char** argv)
{
   try
   {
      CommandLineParser args(argc, argv);
      Log::initialize(args.mLogType, args.mLogLevel, argv[0]);
      resip::Timer::T100 = 0;
      
      TestHolder::createStatic();
      Fixture::initialize(argc, argv);
      
      CppUnit::TextUi::TestRunner runner;

      runner.addTest( MyTestCase::suite() );
      runner.run();
      DebugLog(<< "Finished");

      Fixture::destroyStatic();
   }
   catch (BaseException& e)
   {
      cerr << "Fatal error: " << e << endl;
      exit(-1);
   }

   return 0;
}

// Copyright 2005 Purplecomm, Inc.
/*
  Copyright (c) 2005, PurpleComm, Inc. 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  * Neither the name of PurpleComm, Inc. nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
