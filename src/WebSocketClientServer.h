//
// Created by stephane bourque on 2021-11-01.
//

#ifndef OWPROV_WEBSOCKETCLIENTSERVER_H
#define OWPROV_WEBSOCKETCLIENTSERVER_H

#include "framework/MicroService.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Environment.h"
#include "Poco/NObserver.h"
#include "Poco/Net/SocketNotification.h"

#include "RESTObjects/RESTAPI_SecurityObjects.h"

namespace OpenWifi {

    class MyParallelSocketReactor {
    public:
        explicit MyParallelSocketReactor(unsigned NumReactors = Poco::Environment::processorCount()) :
            NumReactors_(NumReactors) {
            std::cout << __func__ << ":" << __LINE__ << std::endl;

            Reactors_.reserve(NumReactors_);
            std::cout << __func__ << ":" << __LINE__ << std::endl;
            for(int i=0;i<NumReactors_;i++) {
                std::cout << __func__ << ":" << __LINE__ << std::endl;
                Reactors_[i] = std::make_unique<Poco::Net::SocketReactor>();
                std::cout << __func__ << ":" << __LINE__ << std::endl;
                Reactors_[i]->run();
                std::cout << __func__ << ":" << __LINE__ << std::endl;
            }
        }

        ~MyParallelSocketReactor() {
            for(const auto &i:Reactors_)
                i->stop();
        }

        Poco::Net::SocketReactor & Reactor() {
            return *Reactors_[ rand() % NumReactors_ ];
        }

    private:
        unsigned    NumReactors_;
        std::vector<std::unique_ptr<Poco::Net::SocketReactor>>  Reactors_;
    };

    class WebSocketClient;

    class WebSocketClientServer : public SubSystemServer, Poco::Runnable {
        public:
            static WebSocketClientServer *instance() {
                if(instance_== nullptr)
                    instance_ = new WebSocketClientServer;
                return instance_;
            }

            int Start() override;
            void Stop() override;
            void run() override;
            inline MyParallelSocketReactor & ReactorPool() { return *ReactorPool_; }
            inline bool Register( WebSocketClient * Client, const std::string &Id) {
                std::lock_guard G(Mutex_);
                Clients_[Id] = Client;
                return true;
            }

            inline void UnRegister(const std::string &Id) {
                std::lock_guard G(Mutex_);
                Clients_.erase(Id);

            }
            inline bool GeoCodeEnabled() const { return GeoCodeEnabled_; }
            [[nodiscard]] inline const std::string GoogleApiKey() { return GoogleApiKey_; }

        private:
            static WebSocketClientServer *              instance_;
            std::atomic_bool                            Running_=false;
            Poco::Thread                                Thr_;
            std::unique_ptr<MyParallelSocketReactor>    ReactorPool_;
            bool                                        GeoCodeEnabled_=false;
            std::string                                 GoogleApiKey_;
            std::map<std::string,WebSocketClient *>     Clients_;

            WebSocketClientServer() noexcept:
                SubSystemServer("WebSocketClientServer", "WSCLNT-SVR", "websocketclients")
                {
                }
        };

    inline WebSocketClientServer * WebSocketClientServer() { return WebSocketClientServer::instance(); }
    inline class WebSocketClientServer *WebSocketClientServer::instance_ = nullptr;

    class WebSocketClient {
    public:
        explicit WebSocketClient( Poco::Net::WebSocket & WS , const std::string Id, Poco::Logger & L) :
            Reactor_(WebSocketClientServer()->ReactorPool().Reactor()),
            Id_(std::move(Id)),
            Logger_(L) {
            WS_ = std::make_unique<Poco::Net::WebSocket>(WS);
            Reactor_.addEventHandler(*WS_,
                                     Poco::NObserver<WebSocketClient, Poco::Net::ReadableNotification>(
                                             *this, &WebSocketClient::OnSocketReadable));
            Reactor_.addEventHandler(*WS_,
                                     Poco::NObserver<WebSocketClient, Poco::Net::ShutdownNotification>(
                                             *this, &WebSocketClient::OnSocketShutdown));
            Reactor_.addEventHandler(*WS_,
                                     Poco::NObserver<WebSocketClient, Poco::Net::ErrorNotification>(
                                             *this, &WebSocketClient::OnSocketError));
        }

        ~WebSocketClient() {
            Reactor_.removeEventHandler(*WS_,
                                        Poco::NObserver<WebSocketClient,
                                        Poco::Net::ReadableNotification>(*this,&WebSocketClient::OnSocketReadable));
            Reactor_.removeEventHandler(*WS_,
                                        Poco::NObserver<WebSocketClient,
                                        Poco::Net::ShutdownNotification>(*this,&WebSocketClient::OnSocketShutdown));
            Reactor_.removeEventHandler(*WS_,
                                        Poco::NObserver<WebSocketClient,
                                        Poco::Net::ErrorNotification>(*this,&WebSocketClient::OnSocketError));
            (*WS_).shutdown();
            (*WS_).close();
        }

        [[nodiscard]] inline const std::string & Id() { return Id_; };

        std::string GoogleGeoCodeCall(const std::string &A);

        private:
            std::unique_ptr<Poco::Net::WebSocket>     WS_;
            Poco::Net::SocketReactor                & Reactor_;
            std::string                               Id_;
            Poco::Logger                            & Logger_;
            bool                                      Authenticated_=false;
            SecurityObjects::UserInfoAndPolicy        UserInfo_;

            void Process(const Poco::JSON::Object::Ptr &O, std::string &Answer, bool &Done );
            void OnSocketReadable(const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf);
            void OnSocketShutdown(const Poco::AutoPtr<Poco::Net::ShutdownNotification>& pNf);
            void OnSocketError(const Poco::AutoPtr<Poco::Net::ErrorNotification>& pNf);
        };

}

#endif //OWPROV_WEBSOCKETCLIENTSERVER_H
