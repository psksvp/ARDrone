// ARDrone Controller in C++
// by Pongsak Suvanpong psksvp@robotvision2.com or psksvp@gmail.com

#include "Spykee.h"

#include <iostream>

int testMain(int argc, char** argv)
{
  Spykee::Controller co;
  co.connect("192.168.0.14", "admin", "admin[ishere]");
  Spykee::TelemetryData data;
  co.copyTelemetryDataTo(data);
  std::cout << data.name << '\n';
  std::cout << data.firmwareVersion << '\n';
  std::cout << "done";
  return 0;
} 

namespace Spykee 
{
  const int SPYKEE_AUDIO = 1;
	const int SPYKEE_VIDEO_FRAME = 2;
	const int SPYKEE_BATTERY_LEVEL = 3;
	const int SPYKEE_DOCK = 16;
	const int SPYKEE_DOCK_UNDOCKED = 1;
	const int SPYKEE_DOCK_DOCKED = 2;
	const int DEFAULT_VOLUME     = 50;  // volume is between [0, 100]
  
  const char CMD_LOGIN[]       = { 'P', 'K', 0x0a, 0 };
	const char CMD_UNDOCK[]      = { 'P', 'K', 0x10, 0, 1, 5 };
	const char CMD_DOCK[]        = { 'P', 'K', 0x10, 0, 1, 6 };
	const char CMD_CANCEL_DOCK[] = { 'P', 'K', 0x10, 0, 1, 7 };
	const char CMD_START_VIDEO[] = { 'P', 'K', 0x0f, 0, 2, 1, 1 };
	const char CMD_STOP_VIDEO[]  = { 'P', 'K', 0x0f, 0, 2, 1, 0 };
	const char CMD_START_AUDIO[] = { 'P', 'K', 0x0f, 0, 2, 2, 1 };
	const char CMD_STOP_AUDIO[]  = { 'P', 'K', 0x0f, 0, 2, 2, 0 };
	const char mCmdSoundEffect[] = { 'P', 'K', 0x07, 0, 1, 0 };
	const char mCmdSetVolume[]   = { 'P', 'K', 0x09, 0, 1, DEFAULT_VOLUME };
	const char mCmdMove[]        = { 'P', 'K', 0x05, 0, 0x02, 0, 0 };
	
	const int mForwardSpeed = 100;
	const int mBackwardSpeed = 50;
	const int mTurningSpeed = 50;
	
	// The number of characters decoded per line in the hex dump
	const int CHARS_PER_LINE = 32;
	
	// After this many characters in the hex dump, an extra space is inserted
	// (this must be a power of two).
	const int EXTRA_SPACE_FREQ = 8;
	const int EXTRA_SPACE_MASK = EXTRA_SPACE_FREQ - 1;
  
  
  //////////////////////////////
  Controller::Controller()
  {
    myStreamSocket = NULL;
    myThread = NULL;
    myRequestStop = false;
  }
  
  Controller::~Controller()
  {
    disconnect();
  }
  
  bool Controller::connect(const char* strSpykeeHostName,
                           const char* strLoginName, 
                           const char* strPassword)
  {
    try 
    {
      myStreamSocket = new ccxx::StreamSocket;
      myStreamSocket->init();
      myStreamSocket->connect(ccxx::String(strSpykeeHostName), 9000);
      login(strLoginName, strPassword);
      myRequestStop = false;
      myThread = new ccxx::Thread(this);
      myThread->start();
      return true;
    } 
    catch(...) 
    {
      if(NULL != myStreamSocket)
      {
        delete myStreamSocket;
        myStreamSocket = NULL;
      }
      
      if(NULL != myThread)
      {
        delete myThread;
        myThread = NULL;
      }
      return false;
    }
  }
  
  void Controller::disconnect()
  {
    if(NULL == myStreamSocket)
      return;
    try 
    {
      myRequestStop = true;
      while(true == myThread->isRunning())
      {
        ccxx::Thread::currentThread()->sleep(10);
      }
      myStreamSocket->shutdown();
      delete myThread;
      delete myStreamSocket;
      myThread = NULL;
      myStreamSocket = NULL;
      myRequestStop = false;
    }
    catch(...)
    {
      
    }
  }
  
  void Controller::login(const char* strLoginName, const char* strPassword)
  {
    ///send login info
    {
      unsigned char Opk[8000];
      
      Opk[0] = 'P';
      Opk[1] = 'K';
      Opk[2] = 10;
      Opk[3] = 0;
      Opk[4] = strlen(strLoginName) + strlen(strPassword) + 2;
      Opk[5] = strlen(strLoginName);
      strcpy((char *)(Opk+6), strLoginName);
      int x = 6+strlen(strLoginName);
      Opk[x++] = strlen(strPassword);;
      strcpy((char *)(Opk+x), strPassword);
      unsigned long len  = strlen(strLoginName) + strlen(strPassword) + 7;
    
      myStreamSocket->write(Opk, len);
    }
    
    
    /// reading respond from spykee
    {
      MemoryLibrary::StaticBuffer<2048> buffer;
      size_t num = readData(&buffer, 0, 5);
      
      int len = (int)buffer[4];
      num  = readData(&buffer, 0, len);
      // what for???
      /*
      if (len < 8) 
      {
        return;  //connot login
      } */
      
      myTelemetryData.name.clear();
      int pos = 1;
      int nameLen = buffer[pos];
      pos++;
      for(int i = 0; i < nameLen; i++)
      {
        myTelemetryData.name += (char)buffer[pos];
        pos++;
      }
      
      nameLen = buffer[pos];
      pos++;
      for(int i = 0; i < nameLen; i++)
      {
        myTelemetryData.name += (char)buffer[pos];
        pos++;
      }
      
      nameLen = buffer[pos];
      pos++;
      for(int i = 0; i < nameLen; i++)
      {
        myTelemetryData.name += (char)buffer[pos];
        pos++;
      }
      
      myTelemetryData.firmwareVersion.clear();
      nameLen = buffer[pos];
      pos++;
      for(int i = 0; i < nameLen; i++)
      {
        myTelemetryData.firmwareVersion += (char)buffer[pos];
        pos++;
      }
      
      if(0 == buffer[pos])
        myTelemetryData.dockState = eDocked;
      else
        myTelemetryData.dockState = eUnDocked;
    }
  }
  
  size_t Controller::readData(MemoryLibrary::Buffer* buffer, int offset, int len)
  {
    if(offset + len > buffer->size())
    {
      // no good here....  
    }
    
    unsigned char tempBuffer[len];
    size_t byteRead = myStreamSocket->read(tempBuffer, len);
    
    for(int i = 0; i < len; i++)
    {
      buffer->byteValueAt(offset + i) = tempBuffer[i];
    }
    return byteRead;
  }
  
  void Controller::copyTelemetryDataTo(TelemetryData& data)
  {
    ccxx::ScopedLock lock(myMutex);
    myTelemetryData.CopyTo(data);
  }
  
  void Controller::copyImage(MemoryLibrary::DynamicBuffer& buffer)
  {
    ccxx::ScopedLock lock(myMutex);
    buffer.Allocate(myTelemetryData.image.size());
    myTelemetryData.image.CopyTo(buffer);
  }
  
  void Controller::readOnBoardData(bool& bRequestStop)
  {
    MemoryLibrary::StaticBuffer<8192> bytes;
    while(false != bRequestStop)
    {
      int num, len = 0, cmd = -1;
      try 
      {
        ccxx::ScopedLock lock(myMutex);
        num = (int)readData(&bytes, 0, 5);
        if (num == 5 && (bytes[0] & 0xff) == 'P' && (bytes[1] & 0xff) == 'K') 
        {
					cmd = bytes[2] & 0xff;
					len = ((bytes[3] & 0xff) << 8) | (bytes[4] & 0xff);
          
          switch(cmd)
          {
            case SPYKEE_BATTERY_LEVEL:
              readData(&bytes, 5, len);
              myTelemetryData.batteryLevel = bytes[5] & 0xff;
              break;
            
            case SPYKEE_VIDEO_FRAME:
              myTelemetryData.image.Allocate(len);
              readData(&myTelemetryData.image, 0, len);
              break;
            case SPYKEE_AUDIO:
              myTelemetryData.audio.Allocate(len);
              readData(&myTelemetryData.audio, 0, len);
              break;
            case SPYKEE_DOCK:
              myTelemetryData.dockState = eDontKnow;
              readData(&bytes, 5, len);
              myTelemetryData.dockState = ((bytes[5] & 0xff) == SPYKEE_DOCK_DOCKED ? eDocked : eUnDocked);
              break;
            default:
              readData(&bytes, 5, len);
              break;

          } // switch
        } //if
        else
        {
          // unexpected data;
        } //else
      } //try
      catch (...) 
      {
        
      }
    }
  }
  
  
  void Controller::run()
  {
    readOnBoardData(myRequestStop);
  }
} //namespace Spykee

