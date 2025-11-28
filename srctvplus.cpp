// Fix INVALID_HANDLE_VALUE redefinition warning
#ifndef _LINUX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifndef NULL
#define NULL nullptr
#endif
#include "FunctionRoute.h"

#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "game/server/iplayerinfo.h"
#include "eiface.h"
#include "igameevents.h"
#include "convar.h"
#include "Color.h"
#include "vstdlib/random.h"
#include "engine/IEngineTrace.h"
#include "tier2/tier2.h"
#include "ihltv.h"
#include "ihltvdirector.h"
#include "KeyValues.h"
#include "dt_send.h"
#include "server_class.h"

#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IServerGameDLL* g_pGameDLL;
IFileSystem* g_pFileSystem;
IHLTVDirector* g_pHLTVDirector;
IVEngineServer* engine;

class SrcTVPlus : public IServerPluginCallbacks {
public:

  virtual bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;

  // All of the following are just nops or return some static data
  virtual void Unload() override { }
  virtual void Pause() override { }
  virtual void UnPause() override { }
  virtual const char* GetPluginDescription() override { return "srctv+"; }
  virtual void LevelInit(const char* mapName) override { }
  virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override { }
  virtual void GameFrame(bool simulating) override { }
  virtual void LevelShutdown() override { }
  virtual void ClientActive(edict_t* pEntity) override { }
  virtual void ClientDisconnect(edict_t* pEntity) override { }
  virtual void ClientPutInServer(edict_t* pEntity, const char* playername) override { }
  virtual void SetCommandClient(int index) override { }
  virtual void ClientSettingsChanged(edict_t* pEdict) override { }
  virtual PLUGIN_RESULT ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) override { return PLUGIN_CONTINUE; }
  virtual PLUGIN_RESULT ClientCommand(edict_t* pEntity, const CCommand& args) override { return PLUGIN_CONTINUE; }
  virtual PLUGIN_RESULT NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) override { return PLUGIN_CONTINUE; }
  virtual void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) override { }
  virtual void OnEdictAllocated(edict_t* edict) { }
  virtual void OnEdictFreed(const edict_t* edict) { }

private:
  CFunctionRoute m_sendLocalDataTableHook;
  CFunctionRoute m_sendLocalWeaponDataTableHook;
  CFunctionRoute m_sendLocalActiveWeaponDataTableHook;
  static void* SendProxy_SendLocalDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);
  static void* SendProxy_SendLocalWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);
  static void* SendProxy_SendLocalActiveWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);

  static const char** GetModEvents(IHLTVDirector *director);
  CFunctionRoute m_directorHook;
};

SrcTVPlus g_Plugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(SrcTVPlus, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_Plugin);

typedef void* (*SendTableProxyFn)(
    const SendProp *pProp,
    const void *pStructBase,
    const void *pData,
    CSendProxyRecipients *pRecipients,
    int objectID);

// Calls a sendproxy and adds the HLTV pseudo client to the returned recipients list
void* SendProxy_IncludeHLTV(SendTableProxyFn fn, const SendProp* pProp, const void* pStructBase, const void* pData, CSendProxyRecipients* pRecipients, int objectID) {
  if (!fn) {
    Warning("[srctv+] WARNING: SendProxy_IncludeHLTV called with null function pointer\n");
    return nullptr;
  }
  
  if (!pRecipients) {
    Warning("[srctv+] WARNING: SendProxy_IncludeHLTV called with null recipients\n");
    return nullptr;
  }

  const char* ret = (const char*)fn(pProp, pStructBase, pData, pRecipients, objectID);
  
  if (ret) {
    if (engine) {
      if (engine->IsDedicatedServer()) {
        // Normal dedicated server
        if (g_pHLTVDirector) {
          auto server = g_pHLTVDirector->GetHLTVServer();
          if (server) {
            auto slot = server->GetHLTVSlot();
            if (slot >= 0) {
              pRecipients->m_Bits.Set(slot);
            } else {
              Warning("[srctv+] WARNING: HLTV slot is invalid (%d)\n", slot);
            }
          } else {
            Warning("[srctv+] WARNING: GetHLTVServer returned null\n");
          }
        } else {
          Warning("[srctv+] WARNING: g_pHLTVDirector is null in SendProxy_IncludeHLTV\n");
        }
      } else {
        // Listen server
        pRecipients->m_Bits.Set(0);
      }
    } else {
      Warning("[srctv+] WARNING: engine is null in SendProxy_IncludeHLTV\n");
    }
  } else {
    Warning("[srctv+] DEBUG: SendProxy returned null\n");
  }
  return (void*)ret;
}

void* SrcTVPlus::SendProxy_SendLocalDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  auto origFn = g_Plugin.m_sendLocalDataTableHook.CallOriginalFunction<SendTableProxyFn>();
  if (!origFn) {
    Error("[srctv+] ERROR: SendProxy_SendLocalDataTable - original function is null!\n");
    return nullptr;
  }
  return SendProxy_IncludeHLTV(origFn, pProp, pStructBase, pData, pRecipients, objectID);
}

void* SrcTVPlus::SendProxy_SendLocalWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  auto origFn = g_Plugin.m_sendLocalWeaponDataTableHook.CallOriginalFunction<SendTableProxyFn>();
  if (!origFn) {
    Error("[srctv+] ERROR: SendProxy_SendLocalWeaponDataTable - original function is null!\n");
    return nullptr;
  }
  return SendProxy_IncludeHLTV(origFn, pProp, pStructBase, pData, pRecipients, objectID);
}

void* SrcTVPlus::SendProxy_SendLocalActiveWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  auto origFn = g_Plugin.m_sendLocalActiveWeaponDataTableHook.CallOriginalFunction<SendTableProxyFn>();
  if (!origFn) {
    Error("[srctv+] ERROR: SendProxy_SendLocalActiveWeaponDataTable - original function is null!\n");
    return nullptr;
  }
  return SendProxy_IncludeHLTV(origFn, pProp, pStructBase, pData, pRecipients, objectID);
}

// Loads events in a given resource file and inserts them into target set.
bool load_events(const char* filename, std::set<std::string>& target) {
  Warning("[srctv+] DEBUG: Loading events from %s\n", filename);
  auto kv = new KeyValues(filename);
  KeyValues::AutoDelete autodelete_kv(kv);

  if(!kv->LoadFromFile(g_pFileSystem, filename, "GAME")) {
    Warning("[srctv+] WARNING: Could not load events from %s\n", filename);
    return false;
  }

  int count = 0;
  KeyValues* subkey = kv->GetFirstSubKey();
  while(subkey) {
    target.insert(subkey->GetName());
    count++;
    subkey = subkey->GetNextKey();
  }

  Warning("[srctv+] DEBUG: Loaded %d events from %s\n", count, filename);
  return true;
}

// Returns the normal IHLTVDirector::GetModEvents events, along with all other
// events available in the modevents, gameevents, and serverevents resource files.
typedef const char**(*IHLTVDirector_GetModEvents_t)(void *thisPtr);
const char** SrcTVPlus::GetModEvents(IHLTVDirector* director) {
  Warning("[srctv+] DEBUG: GetModEvents hook called\n");
  
  static std::set<std::string> events;
  static std::vector<const char*> list;

  if(!list.empty()) {
    Warning("[srctv+] DEBUG: Returning cached event list (%zu events)\n", list.size());
    return list.data();
  }

  Warning("[srctv+] DEBUG: Building event list...\n");
  auto orig = g_Plugin.m_directorHook.CallOriginalFunction<IHLTVDirector_GetModEvents_t>()(director);
  
  if (!orig) {
    Error("[srctv+] ERROR: GetModEvents - original function returned null!\n");
    return nullptr;
  }
  
  int origCount = 0;
  for(int i = 0; orig[i] != nullptr; i++) {
    events.insert(orig[i]);
    origCount++;
  }
  Warning("[srctv+] DEBUG: Original GetModEvents returned %d events\n", origCount);

  load_events("resource/modevents.res", events);
  load_events("resource/gameevents.res", events);
  load_events("resource/serverevents.res", events);

  for(auto& e : events) {
    list.push_back(e.c_str());
  }
  list.push_back(nullptr);
  list.shrink_to_fit();

  Warning("[srctv+] DEBUG: GetModEvents returning %zu total events\n", list.size());
  return list.data();
}

// Finds a send prop within a send table
SendProp* GetSendPropInTable(SendTable* tbl, const char* propname) {
  for(int i = 0; i < tbl->GetNumProps(); i++) {
    auto prop = tbl->GetProp(i);

    if(!propname) {
      break;
    }

    if(strcmp(prop->GetName(), propname) != 0)
      continue;

    propname = strtok(nullptr, ".");
    if(propname) {
      auto child = prop->GetDataTable();
      if(!child) continue;
      auto ret = GetSendPropInTable(child, propname);
      if(ret) return ret;
    } else {
      return prop;
    }
  }

  return nullptr;
}

// Finds a send prop within a send table, given the class name
SendProp* GetSendProp(const char* tblname, const char* propname) {
  SendTable* tbl = nullptr;
  for(ServerClass* cls = g_pGameDLL->GetAllServerClasses(); cls != nullptr; cls = cls->m_pNext) {
    if(strcmp(cls->m_pNetworkName, tblname) == 0) {
      tbl = cls->m_pTable;
      break;
    }
  }
  if(!tbl) {
    return nullptr;
  }
  char name[256] = { 0 };
  strncpy(name, propname, sizeof(name)-1);
  propname = strtok(name, ".");
  return GetSendPropInTable(tbl, propname);
}

void* search_interface(CreateInterfaceFn factory, const char* name) {
  const char* end = name + strlen(name) - 1;
  int digits = 0;
  while (end > name && isdigit(*end) && digits <= 3) {
    end--;
    digits++;
  }

  if (digits > 0) {
    std::string ifname(name, strlen(name) - digits);

    int max = 1;
    for(auto i = 1; i <= digits; i++) {
      max *= 10;
    }
    for(auto i = 0; i < max; i++) {
      std::stringstream tmp;
      tmp << ifname << std::setfill('0') << std::setw(digits) << i;
      auto ret = factory(tmp.str().c_str(), nullptr);
      if (ret)
        return ret;
    }
  }

  // Fallback: try original name
  return factory(name, nullptr);
}

bool SrcTVPlus::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) {
  Warning("[srctv+] ========== LOAD START ==========");
  Warning("[srctv+] Loading plugin...\n");

  Warning("[srctv+] Searching for INTERFACEVERSION_SERVERGAMEDLL (%s)...\n", INTERFACEVERSION_SERVERGAMEDLL);
  g_pGameDLL = (IServerGameDLL*)search_interface(gameServerFactory, INTERFACEVERSION_SERVERGAMEDLL);
  if(!g_pGameDLL) {
    Error("[srctv+] ERROR: Could not find game DLL interface, aborting load\n");
    return false;
  }
  Warning("[srctv+] ✓ Game DLL interface loaded at %p\n", (void*)g_pGameDLL);

  Warning("[srctv+] Searching for INTERFACEVERSION_VENGINESERVER (%s)...\n", INTERFACEVERSION_VENGINESERVER);
  engine = (IVEngineServer*)search_interface(interfaceFactory, INTERFACEVERSION_VENGINESERVER);
  if (!engine) {
    Error("[srctv+] ERROR: Could not find engine interface, aborting load\n");
    return false;
  }
  Warning("[srctv+] ✓ Engine interface loaded at %p\n", (void*)engine);

  Warning("[srctv+] Searching for FILESYSTEM_INTERFACE_VERSION (%s)...\n", FILESYSTEM_INTERFACE_VERSION);
  g_pFileSystem = (IFileSystem*)search_interface(interfaceFactory, FILESYSTEM_INTERFACE_VERSION);
  if(!g_pFileSystem) {
    Error("[srctv+] ERROR: Could not find filesystem interface, aborting load\n");
    return false;
  }
  Warning("[srctv+] ✓ FileSystem interface loaded at %p\n", (void*)g_pFileSystem);

  Warning("[srctv+] Searching for INTERFACEVERSION_HLTVDIRECTOR (%s)...\n", INTERFACEVERSION_HLTVDIRECTOR);
  g_pHLTVDirector = (IHLTVDirector*)search_interface(gameServerFactory, INTERFACEVERSION_HLTVDIRECTOR);
  if(!g_pHLTVDirector) {
    Error("[srctv+] ERROR: Could not find SrcTV director, aborting load\n");
    return false;
  }
  Warning("[srctv+] ✓ HLTV Director interface loaded at %p\n", (void*)g_pHLTVDirector);

  Warning("[srctv+] Attempting to hook IHLTVDirector::GetModEvents...\n");
  if (!m_directorHook.RouteVirtualFunction(g_pHLTVDirector, &IHLTVDirector::GetModEvents, SrcTVPlus::GetModEvents)) {
    Error("[srctv+] ERROR: Failed to hook IHLTVDirector::GetModEvents\n");
    return false;
  }
  Warning("[srctv+] ✓ GetModEvents hook installed\n");

  Warning("[srctv+] ========== HOOK SETUP START ==========");
  Warning("[srctv+] Hooking SendProxies for HLTV data propagation...\n");

  Warning("[srctv+] [1/3] Looking up CBasePlayer.localdata...\n");
  auto prop = GetSendProp("CBasePlayer", "localdata");
  if(!prop) {
    Error("[srctv+] ERROR: Could not find CBasePlayer prop 'localdata'\n");
    return false;
  }
  Warning("[srctv+]   ✓ Found prop at %p\n", (void*)prop);
  void* proxyFn = (void*)prop->GetDataTableProxyFn();
  Warning("[srctv+]   ✓ Proxy function at %p\n", proxyFn);
  if (!m_sendLocalDataTableHook.RouteFunction(proxyFn, (void*)&SrcTVPlus::SendProxy_SendLocalDataTable)) {
    Error("[srctv+] ERROR: Failed to hook SendProxy_SendLocalDataTable\n");
    return false;
  }
  Warning("[srctv+]   ✓ Hook installed successfully\n");

  Warning("[srctv+] [2/3] Looking up CBaseCombatWeapon.LocalWeaponData...\n");
  prop = GetSendProp("CBaseCombatWeapon", "LocalWeaponData");
  if(!prop) {
    Error("[srctv+] ERROR: Could not find CBaseCombatWeapon prop 'LocalWeaponData'\n");
    return false;
  }
  Warning("[srctv+]   ✓ Found prop at %p\n", (void*)prop);
  proxyFn = (void*)prop->GetDataTableProxyFn();
  Warning("[srctv+]   ✓ Proxy function at %p\n", proxyFn);
  if (!m_sendLocalWeaponDataTableHook.RouteFunction(proxyFn, (void*)&SrcTVPlus::SendProxy_SendLocalWeaponDataTable)) {
    Error("[srctv+] ERROR: Failed to hook SendProxy_SendLocalWeaponDataTable\n");
    return false;
  }
  Warning("[srctv+]   ✓ Hook installed successfully\n");

  Warning("[srctv+] [3/3] Looking up CBaseCombatWeapon.LocalActiveWeaponData...\n");
  prop = GetSendProp("CBaseCombatWeapon", "LocalActiveWeaponData");
  if(!prop) {
    Error("[srctv+] ERROR: Could not find CBaseCombatWeapon prop 'LocalActiveWeaponData'\n");
    return false;
  }
  Warning("[srctv+]   ✓ Found prop at %p\n", (void*)prop);
  proxyFn = (void*)prop->GetDataTableProxyFn();
  Warning("[srctv+]   ✓ Proxy function at %p\n", proxyFn);
  if (!m_sendLocalActiveWeaponDataTableHook.RouteFunction(proxyFn, (void*)&SrcTVPlus::SendProxy_SendLocalActiveWeaponDataTable)) {
    Error("[srctv+] ERROR: Failed to hook SendProxy_SendLocalActiveWeaponDataTable\n");
    return false;
  }
  Warning("[srctv+]   ✓ Hook installed successfully\n");

  Warning("[srctv+] ========== LOAD COMPLETE ==========");
  Warning("[srctv+] ✓ Plugin loaded successfully!\n");
  return true;
}

