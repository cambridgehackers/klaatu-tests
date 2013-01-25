
enum {
    org.bluez.Error.DeviceUnreachable,
    org.bluez.Error.AlreadyConnected,
    org.bluez.Error.ConnectionAttemptFailed,
    org.bluez.Error.NotConnected,
    org.bluez.Error.InProgress,
    org.bluez.Error.InvalidArguments,
    org.bluez.Error.OutOfMemory,
    org.bluez.Error.NotAvailable,
    org.bluez.Error.NotSupported,
    org.bluez.Error.AlreadyExists,
    org.bluez.Error.DoesNotExist,
    org.bluez.Error.Canceled,
    org.bluez.Error.Failed,
    org.bluez.Error.NotReady,
    org.bluez.Error.UnknwownMethod,
    org.bluez.Error.NotAuthorized,
    org.bluez.Error.Rejected,
    org.bluez.Error.NoSuchAdapter,
    org.bluez.Error.NoSuchService,
    org.bluez.Error.RequestDeferred,
    org.bluez.Error.NotInProgress,
    org.bluez.Error.UnsupportedMajorClass,
    org.bluez.Error.AuthenticationCanceled,
    org.bluez.Error.AuthenticationFailed,
    org.bluez.Error.AuthenticationTimeout,
    org.bluez.Error.AuthenticationRejected,
    org.bluez.Error.RepeatedAttempts, };

interface org.bluez.Manager {
    uint32 InterfaceVersion();
    string DefaultAdapter();
    string FindAdapter(string pattern);
    string[] ListAdapters();
    string FindService(string pattern);
    string[] ListServices();
    string ActivateService(string pattern);
}
interface org.bluez.Manager.signals {
    void AdapterAdded(string path);
    void AdapterRemoved(string path);
    void DefaultAdapterChanged(string path);
    void ServiceAdded(string path);
    void ServiceRemoved(string path);
}
interface org.bluez.Database {
    uint32 AddServiceRecord(byte[]);
    uint32 AddServiceRecordFromXML(string record);
    void UpdateServiceRecord(uint32 handle, byte[]);
    void UpdateServiceRecordFromXML(uint32 handle, string record);
    void RemoveServiceRecord(uint32 handle);
}
interface org.bluez.Adapter {
    // Object path	/org/bluez/{hci0,hci1,...}
    dict GetInfo();
    string GetAddress();
    string GetVersion();
    string GetRevision();
    string GetManufacturer();
    string GetCompany();
    string[] ListAvailableModes();
    string GetMode();
    void SetMode(string mode);
    uint32 GetDiscoverableTimeout();
    void SetDiscoverableTimeout(uint32 timeout);
    boolean IsConnectable();
    boolean IsDiscoverable();
    boolean IsConnected(string address);
    string[] ListConnections();
    string GetMajorClass();
    string[] ListAvailableMinorClasses();
    string GetMinorClass();
    void SetMinorClass(string minor);
    string[] GetServiceClasses();
    string GetName();
    void SetName(string name);
    dict GetRemoteInfo(string address);
    string GetRemoteVersion(string address);
    string GetRemoteRevision(string address);
    string GetRemoteManufacturer(string address);
    string GetRemoteCompany(string address);
    string GetRemoteMajorClass(string address);
    string GetRemoteMinorClass(string address);
    string[] GetRemoteServiceClasses(string address);
    uint32 GetRemoteClass(string address);
    byte[] GetRemoteFeatures(string address);
    string GetRemoteName(string address);
    string GetRemoteAlias(string address);
    void SetRemoteAlias(string address, string alias);
    void ClearRemoteAlias(string address);
    string LastSeen(string address);
    string LastUsed(string address);
    void DisconnectRemoteDevice(string address);
    void CreateBonding(string address);
    void CancelBondingProcess(string address);
    void RemoveBonding(string address);
    boolean HasBonding(string address);
    string[] ListBondings();
    uint8 GetPinCodeLength(string address);
    uint8 GetEncryptionKeySize(string address);
    void SetTrusted(string address);
    boolean IsTrusted(string address);
    void RemoveTrust(string address);
    string[] ListTrusts();
    void DiscoverDevices();
    void DiscoverDevicesWithoutNameResolving();
    void CancelDiscovery();
    void StartPeriodicDiscovery();
    void StopPeriodicDiscovery();
    boolean IsPeriodicDiscovery();
    void SetPeriodicDiscoveryNameResolving(boolean resolve_names);
    boolean GetPeriodicDiscoveryNameResolving();
    uint32[] GetRemoteServiceHandles(string address, string match);
    byte[] GetRemoteServiceRecord(string address, uint32 handle);
    string GetRemoteServiceRecordAsXML(string address, uint32 handle);
    string[] GetRemoteServiceIdentifiers(string address);
    void FinishRemoteServiceTransaction(string address);
    string[] ListRemoteDevices();
    string[] ListRecentRemoteDevices(string date);
}
interface org.bluez.Adapter.signals {
    void ModeChanged(string mode);
    void DiscoverableTimeoutChanged(uint32 timeout);
    void MinorClassChanged(string minor);
    void NameChanged(string name);
    void DiscoveryStarted();
    void DiscoveryCompleted();
    void PeriodicDiscoveryStarted();
    void PeriodicDiscoveryStopped();
    void RemoteDeviceFound(string address, uint32 class, int16 rssi);
    void RemoteDeviceDisappeared(string address);
    void RemoteClassUpdated(string address, uint32 class);
    void RemoteNameUpdated(string address, string name);
    void RemoteIdentifiersUpdated(string address, string[]);
    void RemoteNameFailed(string address);
    void RemoteNameRequested(string address);
    void RemoteAliasChanged(string address, string alias);
    void RemoteAliasCleared(string address);
    void RemoteDeviceConnected(string address);
    void RemoteDeviceDisconnectRequested(string address);
    void RemoteDeviceDisconnected(string address);
    void BondingCreated(string address);
    void BondingRemoved(string address);
    void TrustAdded(string address);
    void TrustRemoved(string address);
}
interface org.bluez.Service {
    // Object path	path from org.bluez.Manager.ListServices();
    dict GetInfo();
    string GetIdentifier();
    string GetName();
    string GetDescription();
    string GetBusName(); // [experimental]
    void Start();
    void Stop();
    boolean IsRunning();
    boolean IsExternal();
    string[] ListTrusts(); // [experimental]
    void SetTrusted(string address); // [experimental]
    boolean IsTrusted(string address); // [experimental]
    void RemoveTrust(string address); // [experimental]
}
interface org.bluez.Service.signals {
    void Started();
    void Stopped();
    void TrustAdded(string address);
    void TrustRemoved(string address);
}
interface org.bluez.Security {
    // Object path	/org/bluez or /org/bluez/{hci0,hci1,...}
    void RegisterDefaultPasskeyAgent(string path);
    void UnregisterDefaultPasskeyAgent(string path);
    void RegisterPasskeyAgent(string path, string address);
    void UnregisterPasskeyAgent(string path, string address);
    void RegisterDefaultAuthorizationAgent(string path);
    void UnregisterDefaultAuthorizationAgent(string path);
}
interface org.bluez.PasskeyAgent {
    // Service      unique name
    // Object path	freely definable
    string Request(string path, string address);
    void Cancel(string path, string address);
    void Release();
}
interface org.bluez.AuthorizationAgent { // experimental
    // Object path	freely definable
    void Authorize(string adapter_path, string address, string service_path, string uuid);
    void Cancel(string adapter_path, string address, string service_path, string uuid);
    void Release();
}
