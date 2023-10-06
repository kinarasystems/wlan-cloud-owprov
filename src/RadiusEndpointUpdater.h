//
// Created by stephane bourque on 2023-10-02.
//

#pragma once
#include <framework/AppServiceRegistry.h>
#include <framework/utils.h>
#include <StorageService.h>
#include <RadiusEndpointTypes/OrionWifi.h>
#include <RadiusEndpointTypes/GlobalReach.h>
#include <sdks/SDK_gw.h>
#include <RESTObjects/RESTAPI_GWobjects.h>

namespace OpenWifi {
    class RadiusEndpointUpdater {
    public:

        void ParseCertChain(const std::string &Chain, std::vector<std::string> &ChainVec) {
            std::istringstream os(Chain);
            std::string CurrentCert;
            bool InCert = false;
            while(os.good()) {
                std::string Line;
                os >> Line;
                std::cout << Line << std::endl;
                if(Line=="-----BEGIN CERTIFICATE-----") {
                    InCert = true;
                    CurrentCert += Line;
                    CurrentCert += "\n";
                    continue;
                }
                if(Line=="-----END CERTIFICATE-----" && InCert) {
                    InCert = false;
                    CurrentCert += Line;
                    CurrentCert += "\n";
                    ChainVec.emplace_back(CurrentCert);
                    continue;
                }
                if(InCert) {
                    CurrentCert += Line;
                    CurrentCert += "\n";
                }
            }
        }

        inline bool UpdateEndpoints( RESTAPIHandler *Client, [[maybe_unused]] std::string & Error,
                                     [[maybe_unused]] uint64_t &ErrorNum  ) {

            std::vector<ProvObjects::RADIUSEndPoint>    Endpoints;
            GWObjects::RadiusProxyPoolList  Pools;
            StorageService()->RadiusEndpointDB().GetRecords(0,500,Endpoints);

            for(const auto &Endpoint:Endpoints) {
                GWObjects::RadiusProxyPool  PP;

                PP.name = Endpoint.info.name;
                PP.description = Endpoint.info.description;
                PP.useByDefault = false;
                PP.poolProxyIp = Endpoint.Index;
                PP.radsecKeepAlive = 25;
                PP.enabled = true;

                if(Endpoint.Type=="orion" && !Endpoint.RadsecServers.empty()) {
                    auto Svrs = OpenRoaming_Orion()->GetServers();
                    PP.radsecPoolType="orion";
                    ProvObjects::GooglOrionAccountInfo  OA;
                    if(StorageService()->OrionAccountsDB().GetRecord("id", Endpoint.RadsecServers[0].UseOpenRoamingAccount, OA)) {
                        for(auto *ServerType:{&PP.authConfig, &PP.acctConfig, &PP.coaConfig}) {
                            ServerType->monitor = false;
                            ServerType->strategy = Endpoint.PoolStrategy;
                            ServerType->monitorMethod = "none";
                            ServerType->strategy = "random";
                            int i=1;
                            for (const auto &Server: Svrs) {
                                GWObjects::RadiusProxyServerEntry PE;
                                PE.radsecCert = Utils::base64encode((const u_char *)OA.certificate.c_str(),OA.certificate.size());
                                PE.radsecKey = Utils::base64encode((const u_char *)OA.privateKey.c_str(),OA.privateKey.size());
                                for(const auto &cert:OA.cacerts) {
                                    PE.radsecCacerts.emplace_back(Utils::base64encode((const u_char *)cert.c_str(),cert.size()));
                                }
                                PE.radsec = true;
                                PE.name = fmt::format("Server {}",i++);
                                PE.ignore = false;
                                PE.ip = Server.Hostname;
                                PE.port = PE.radsecPort = Server.Port;
                                PE.allowSelfSigned = false;
                                PE.weight = 10;
                                PE.secret = PE.radsecSecret = "radsec";
                                ServerType->servers.emplace_back(PE);
                            }
                        }
                        Pools.pools.emplace_back(PP);
                    }
                } else if(Endpoint.Type=="globalreach" && !Endpoint.RadsecServers.empty()) {
                    auto Svrs = OpenRoaming_GlobalReach()->GetServers();
                    PP.radsecPoolType="globalreach";
                    ProvObjects::GLBLRCertificateInfo   GRCertificate;
                    ProvObjects::GLBLRAccountInfo       GRAccountInfo;
                    if( StorageService()->GLBLRCertsDB().GetRecord("id",Endpoint.RadsecServers[0].UseOpenRoamingAccount,GRCertificate) &&
                        StorageService()->GLBLRAccountInfoDB().GetRecord("id",GRCertificate.accountId,GRAccountInfo)) {
                        for(auto *ServerType:{&PP.authConfig, &PP.acctConfig, &PP.coaConfig}) {
                            ServerType->monitor = false;
                            ServerType->monitorMethod = "none";
                            ServerType->strategy = Endpoint.PoolStrategy;
                            ServerType->strategy = "random";
                            int i = 1;
                            for (const auto &Server: Svrs) {
                                GWObjects::RadiusProxyServerEntry PE;
                                PE.radsecCert = Utils::base64encode((const u_char *)GRCertificate.certificate.c_str(),GRCertificate.certificate.size());
                                PE.radsecKey = Utils::base64encode((const u_char *)GRAccountInfo.CSRPrivateKey.c_str(),GRAccountInfo.CSRPrivateKey.size());
                                std::vector<std::string> Chain;
                                ParseCertChain(GRCertificate.certificateChain,Chain);
                                for(const auto &cert:Chain) {
                                    PE.radsecCacerts.emplace_back( Utils::base64encode((const u_char *)cert.c_str(),cert.size()));
                                }
                                PE.radsec = true;
                                PE.name = fmt::format("Server {}", i++);
                                PE.ignore = false;
                                PE.ip = Server.Hostname;
                                PE.port = PE.radsecPort = Server.Port;
                                PE.allowSelfSigned = false;
                                PE.weight = 10;
                                PE.secret = PE.radsecSecret = "radsec";
                                ServerType->servers.emplace_back(PE);
                            }
                        }
                        Pools.pools.emplace_back(PP);
                    }
                } else if(Endpoint.Type=="radsec"  && !Endpoint.RadsecServers.empty()) {
                    PP.radsecPoolType="radsec";
                    for(auto *ServerType:{&PP.authConfig, &PP.acctConfig, &PP.coaConfig}) {
                        ServerType->monitor = false;
                        ServerType->strategy = Endpoint.PoolStrategy;
                        ServerType->monitorMethod = "none";
                        ServerType->strategy = "random";
                        for (const auto &Server: Endpoint.RadsecServers) {
                            GWObjects::RadiusProxyServerEntry PE;
                            PE.radsecCert = Utils::base64encode((const u_char *)Server.Certificate.c_str(), Server.Certificate.size());
                            PE.radsecKey = Utils::base64encode((const u_char *)Server.PrivateKey.c_str(),Server.PrivateKey.size());
                            for(const auto &C:Server.CaCerts) {
                                PE.radsecCacerts.emplace_back(Utils::base64encode(
                                        (const u_char *) C.c_str(),
                                        C.size()));
                            }
                            PE.radsec = true;
                            PE.name = Server.Hostname;
                            PE.ignore = false;
                            PE.ip = Server.IP;
                            PE.port = PE.radsecPort = Server.Port;
                            PE.allowSelfSigned = false;
                            PE.weight = 10;
                            PE.secret = PE.radsecSecret = "radsec";
                            ServerType->servers.emplace_back(PE);
                        }
                    }
                    Pools.pools.emplace_back(PP);
                } else if(Endpoint.Type=="generic"  && !Endpoint.RadiusServers.empty()) {
                    PP.radsecPoolType="generic";
/*                    const auto &server = Endpoint.RadiusServers[0];
                    for(auto &[ServerType,ServerInfo] : {
                            {}AUTH, Endpoint.RadiusServers[0].Authentication[0]) ,
                            std::pair(ACCT, Endpoint.RadiusServers[0].Accounting[0] ),
                            std::pair(COA, Endpoint.RadiusServers[0].Accounting[0]) }
                     ) {
                        ServerType.
                        ServerType.monitor = false;
                        ServerType.strategy = Endpoint.PoolStrategy;
                        GWObjects::RadiusProxyServerEntry PE;
                        PE.radsec = false;
                        PE.name = ServerInfo.Hostname;
                        PE.ignore = false;
                        PE.ip = ServerInfo.IP;
                        PE.port = PE.radsecPort = ServerInfo.Port;
                        PE.allowSelfSigned = false;
                        PE.weight = 10;
                        PE.secret = PE.radsecSecret = "radsec";
                        ServerType.servers.emplace_back(PE);
                    }
                    Pools.pools.emplace_back(PP);
  */              }
            }

            Poco::JSON::Object  oo;
            Pools.to_json(oo);
            oo.stringify(std::cout,2,2);

            GWObjects::RadiusProxyPoolList  NewPools;
            Poco::JSON::Object ErrorObj;
            if(SDK::GW::RADIUS::SetConfiguration(Client, Pools, NewPools, ErrorObj)) {
                DBGLINE
                AppServiceRegistry().Set("radiusEndpointLastUpdate", Utils::Now());
                DBGLINE
                return true;
            }

            DBGLINE
            Error = "Could not update the controller.";
            ErrorNum = 1;

            DBGLINE

            return false;
        }

    private:

    };



} // OpenWifi
