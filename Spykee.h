// ARDrone Controller in C++
// by Pongsak Suvanpong psksvp@robotvision2.com or psksvp@gmail.com

#include <string>
#include <commonc++/StreamSocket.h++>
#include <commonc++/Mutex.h++>
#include <commonc++/Thread.h++>
#include <commonc++/Common.h++>
#include <commonc++/Lock.h++>
#include <commonc++/String.h++>
#include <commonc++/ScopedLock.h++>
#include "../Common/MemoryLibrary.h"

namespace Spykee
{
  enum DockState 
  {
    eDontKnow,
    eDocked,
    eUnDocked,
    eDocking
  };
  
  struct TelemetryData
  {
    std::string name;
    std::string firmwareVersion;
    DockState dockState;
    int batteryLevel;
    MemoryLibrary::DynamicBuffer image;
    MemoryLibrary::DynamicBuffer audio;
    
    void CopyTo(TelemetryData& data)
    {
      data.name = name;
      data.firmwareVersion = firmwareVersion;
      data.dockState = dockState;
      data.batteryLevel = batteryLevel;
      data.image.Allocate(image.size());
      image.CopyTo(data.image);
      data.audio.Allocate(audio.size());
      audio.CopyTo(data.audio);
    }
  };
  
    
  class Controller :public ccxx::Runnable
  {
  public:  
    Controller();
    ~Controller();
    
    bool connect(const char* strSpykeeHostName,
                 const char* strLoginName, 
                 const char* strPassword);
    
    void disconnect();
    bool isConnected() {return myStreamSocket != NULL;}
    
    void copyTelemetryDataTo(TelemetryData& data);
    void copyImage(MemoryLibrary::DynamicBuffer& buffer);
    
  protected:
    void login(const char* strLoginName, const char* strPassword);
    size_t readData(MemoryLibrary::Buffer* buffer, int offset, int len);
    
  private:
    void run();
    void readOnBoardData(bool& bRequestStop);
    
    ccxx::Mutex myMutex;
    bool myRequestStop;
    
    TelemetryData myTelemetryData;
    
    ccxx::StreamSocket* myStreamSocket;
    ccxx::Thread* myThread;
  };
  
  
  
} //namespace Spykee

