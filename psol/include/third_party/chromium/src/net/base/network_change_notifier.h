// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_

#include "base/basictypes.h"
#include "base/observer_list_threadsafe.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

struct DnsConfig;
class HistogramWatcher;
class NetworkChangeNotifierFactory;

#if defined(OS_LINUX)
namespace internal {
class AddressTrackerLinux;
}
#endif

// NetworkChangeNotifier monitors the system for network changes, and notifies
// registered observers of those events.  Observers may register on any thread,
// and will be called back on the thread from which they registered.
// NetworkChangeNotifiers are threadsafe, though they must be created and
// destroyed on the same thread.
class NET_EXPORT NetworkChangeNotifier {
 public:
  // Using the terminology of the Network Information API:
  // http://www.w3.org/TR/netinfo-api.
  enum ConnectionType {
    CONNECTION_UNKNOWN = 0, // A connection exists, but its type is unknown.
    CONNECTION_ETHERNET = 1,
    CONNECTION_WIFI = 2,
    CONNECTION_2G = 3,
    CONNECTION_3G = 4,
    CONNECTION_4G = 5,
    CONNECTION_NONE = 6     // No connection.
  };

  class NET_EXPORT IPAddressObserver {
   public:
    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    IPAddressObserver() {}
    virtual ~IPAddressObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(IPAddressObserver);
  };

  class NET_EXPORT ConnectionTypeObserver {
   public:
    // Will be called when the connection type of the system has changed.
    // See NetworkChangeNotifier::GetConnectionType() for important caveats
    // about the unreliability of using this signal to infer the ability to
    // reach remote sites.
    virtual void OnConnectionTypeChanged(ConnectionType type) = 0;

   protected:
    ConnectionTypeObserver() {}
    virtual ~ConnectionTypeObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectionTypeObserver);
  };

  class NET_EXPORT DNSObserver {
   public:
    // Will be called when the DNS settings of the system may have changed.
    // Use GetDnsConfig to obtain the current settings.
    virtual void OnDNSChanged() = 0;

   protected:
    DNSObserver() {}
    virtual ~DNSObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DNSObserver);
  };

  virtual ~NetworkChangeNotifier();

  // See the description of NetworkChangeNotifier::GetConnectionType().
  // Implementations must be thread-safe. Implementations must also be
  // cheap as this could be called (repeatedly) from the network thread.
  virtual ConnectionType GetCurrentConnectionType() const = 0;

  // Replaces the default class factory instance of NetworkChangeNotifier class.
  // The method will take over the ownership of |factory| object.
  static void SetFactory(NetworkChangeNotifierFactory* factory);

  // Creates the process-wide, platform-specific NetworkChangeNotifier.  The
  // caller owns the returned pointer.  You may call this on any thread.  You
  // may also avoid creating this entirely (in which case nothing will be
  // monitored), but if you do create it, you must do so before any other
  // threads try to access the API below, and it must outlive all other threads
  // which might try to use it.
  static NetworkChangeNotifier* Create();

  // Returns the connection type.
  // A return value of |CONNECTION_NONE| is a pretty strong indicator that the
  // user won't be able to connect to remote sites. However, another return
  // value doesn't imply that the user will be able to connect to remote sites;
  // even if some link is up, it is uncertain whether a particular connection
  // attempt to a particular remote site will be successful.
  static ConnectionType GetConnectionType();

  // Retrieve the last read DnsConfig. This could be expensive if the system has
  // a large HOSTS file.
  static void GetDnsConfig(DnsConfig* config);

#if defined(OS_LINUX)
  // Returns the AddressTrackerLinux if present.
  static const internal::AddressTrackerLinux* GetAddressTracker();
#endif

  // Convenience method to determine if the user is offline.
  // Returns true if there is currently no internet connection.
  //
  // A return value of |true| is a pretty strong indicator that the user
  // won't be able to connect to remote sites. However, a return value of
  // |false| is inconclusive; even if some link is up, it is uncertain
  // whether a particular connection attempt to a particular remote site
  // will be successfully.
  static bool IsOffline() {
    return GetConnectionType() == CONNECTION_NONE;
  }

  // Like Create(), but for use in tests.  The mock object doesn't monitor any
  // events, it merely rebroadcasts notifications when requested.
  static NetworkChangeNotifier* CreateMock();

  // Registers |observer| to receive notifications of network changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.  This is safe to call if Create() has not
  // been called (as long as it doesn't race the Create() call on another
  // thread), in which case it will simply do nothing.
  static void AddIPAddressObserver(IPAddressObserver* observer);
  static void AddConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void AddDNSObserver(DNSObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.  Like AddObserver(),
  // this is safe to call if Create() has not been called (as long as it doesn't
  // race the Create() call on another thread), in which case it will simply do
  // nothing.  Technically, it's also safe to call after the notifier object has
  // been destroyed, if the call doesn't race the notifier's destruction, but
  // there's no reason to use the API in this risky way, so don't do it.
  static void RemoveIPAddressObserver(IPAddressObserver* observer);
  static void RemoveConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void RemoveDNSObserver(DNSObserver* observer);

  // Allow unit tests to trigger notifications.
  static void NotifyObserversOfIPAddressChangeForTests() {
    NotifyObserversOfIPAddressChange();
  }

  // Let the NetworkChangeNotifier know we received some data.
  // This is used strictly for producing histogram data about the accuracy of
  // the NetworkChangenotifier's online detection.
  static void NotifyDataReceived(const GURL& source);

  // Register the Observer callbacks for producing histogram data.  This
  // should be called from the network thread to avoid race conditions.
  static void InitHistogramWatcher();

 protected:
  NetworkChangeNotifier();

#if defined(OS_LINUX)
  // Returns the AddressTrackerLinux if present.
  // TODO(szym): Retrieve AddressMap from NetworkState. http://crbug.com/144212
  virtual const internal::AddressTrackerLinux*
      GetAddressTrackerInternal() const;
#endif

  // Broadcasts a notification to all registered observers.  Note that this
  // happens asynchronously, even for observers on the current thread, even in
  // tests.
  static void NotifyObserversOfIPAddressChange();
  static void NotifyObserversOfConnectionTypeChange();
  static void NotifyObserversOfDNSChange();

  // Stores |config| in NetworkState and notifies observers.
  static void SetDnsConfig(const DnsConfig& config);

 private:
  friend class HostResolverImplDnsTest;
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierLinuxTest;
  friend class NetworkChangeNotifierWinTest;

  class NetworkState;

  // Allows a second NetworkChangeNotifier to be created for unit testing, so
  // the test suite can create a MockNetworkChangeNotifier, but platform
  // specific NetworkChangeNotifiers can also be created for testing.  To use,
  // create an DisableForTest object, and then create the new
  // NetworkChangeNotifier object.  The NetworkChangeNotifier must be
  // destroyed before the DisableForTest object, as its destruction will restore
  // the original NetworkChangeNotifier.
  class NET_EXPORT_PRIVATE DisableForTest {
   public:
    DisableForTest();
    ~DisableForTest();

   private:
    // The original NetworkChangeNotifier to be restored on destruction.
    NetworkChangeNotifier* network_change_notifier_;
  };

  const scoped_refptr<ObserverListThreadSafe<IPAddressObserver> >
      ip_address_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<ConnectionTypeObserver> >
      connection_type_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<DNSObserver> >
      resolver_state_observer_list_;

  // The current network state. Hosts DnsConfig, exposed via GetDnsConfig.
  scoped_ptr<NetworkState> network_state_;

  // A little-piggy-back observer that simply logs UMA histogram data.
  scoped_ptr<HistogramWatcher> histogram_watcher_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
