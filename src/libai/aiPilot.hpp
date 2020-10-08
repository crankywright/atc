// 
// This file is part of AT&C project which simulates virtual world of air traffic and ATC.
// Code licensing terms are available at https://github.com/felix-b/atc/blob/master/LICENSE
// 
#include <chrono>
#include <sstream>
#include <iomanip>
#include "libworld.h"
#include "worldHelper.hpp"
#include "basicManeuverTypes.hpp"
#include "maneuverFactory.hpp"
#include "intentTypes.hpp"
#include "intentFactory.hpp"
#include "libai.hpp"

using namespace std;
using namespace world;

namespace ai
{
    class AIPilot : public Pilot
    {
    private:
        WorldHelper m_helper;
        shared_ptr<ManeuverFactory> m_maneuverFactory;
        shared_ptr<IntentFactory> m_intentFactory;
        ManeuverFactory& M;
        IntentFactory& I;
        shared_ptr<Airport> m_departureAirport;
        shared_ptr<FlightPlan> m_flightPlan;
        int m_departureTowerKhz = 0;
        int m_departureKhz = 0;
        int m_arrivalGroundKhz = 0;
        uint64_t m_lastReceivedIntentId = 0;
    public:
        AIPilot(
            shared_ptr<HostServices> _host, 
            int _id, 
            Actor::Gender _gender, 
            shared_ptr<Flight> _flight, 
            shared_ptr<ManeuverFactory> _maneuverFactory,
            shared_ptr<IntentFactory> _intentFactory
        ) : Pilot(_host, _id, _gender, _flight),
            m_helper(_host),
            m_maneuverFactory(_maneuverFactory),
            M(*_maneuverFactory),
            m_intentFactory(_intentFactory),
            I(*_intentFactory)
        {
            //_host->writeLog("AIPilot::AIPilot() - enter");

            m_flightPlan = _flight->plan();
            m_departureAirport = _host->getWorld()->getAirport(_flight->plan()->departureAirportIcao());

            aircraft()->onCommTransmission([this](shared_ptr<Intent> intent) {
                handleCommTransmission(intent);
            });

            //_host->writeLog("AIPilot::AIPilot() - exit");
        }
    public:
        shared_ptr<Maneuver> getFlightCycle() override
        {
            return maneuverFlightCycle();
        }
        shared_ptr<Maneuver> getFinalToGate(const Runway::End& landingRunway) override
        {
            return maneuverFinalToGate(landingRunway);
        }
        void progressTo(chrono::microseconds timestamp) override 
        {
            //TODO
        }
    private:
        void handleCommTransmission(shared_ptr<Intent> intent)
        {
            if (intent->direction() == Intent::Direction::ControllerToPilot && intent->subjectFlight() == flight())
            {
                host()->writeLog("TRANSMISSION HANDLED BY PILOT [%s]", flight()->callSign().c_str(), intent->code());
                shared_ptr<Clearance> newClearance;
                m_lastReceivedIntentId = intent->id();

                switch (intent->code())
                {
                case DeliveryIfrClearanceReplyIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<DeliveryIfrClearanceReplyIntent>(intent)->clearance();
                    break;
                case DeliveryIfrClearanceReadbackCorrectIntent::IntentCode:
                    {
                        auto readbackCorrect = dynamic_pointer_cast<DeliveryIfrClearanceReadbackCorrectIntent>(intent);
                        auto clearance = readbackCorrect->clearance();
                        // auto delivery = clearance->header().issuedBy;
                        // auto intentFactory = host()->services().get<IntentFactory>();
                        // auto handoffReadback = intentFactory->pilotHandoffReadback(flight(), delivery, readbackCorrect->groundKhz());
                        //delivery->frequency()->enqueueTransmission(handoffReadback);
                        clearance->setReadbackCorrect();
                    }
                    break;
                case GroundPushAndStartReplyIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<GroundPushAndStartReplyIntent>(intent)->approval();
                    break;
                case GroundDepartureTaxiReplyIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<GroundDepartureTaxiReplyIntent>(intent)->clearance();
                    break;
                case GroundRunwayCrossClearanceIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<GroundRunwayCrossClearanceIntent>(intent)->clearance();
                    break;
                case GroundSwitchToTowerIntent::IntentCode:
                    m_departureTowerKhz = dynamic_pointer_cast<GroundSwitchToTowerIntent>(intent)->towerKhz();
                    break;
                case TowerLineUpIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<TowerLineUpIntent>(intent)->approval();
                    break;
                case TowerClearedForTakeoffIntent::IntentCode:
                    {
                        auto takeOffClearance = dynamic_pointer_cast<TowerClearedForTakeoffIntent>(intent)->clearance();
                        m_departureKhz = takeOffClearance->departureKhz();
                        newClearance = takeOffClearance;
                    }
                    break;
                case TowerClearedForLandingIntent::IntentCode:
                    {
                        auto landingClearance = dynamic_pointer_cast<TowerClearedForLandingIntent>(intent)->clearance();
                        m_arrivalGroundKhz = landingClearance->groundKhz();
                        newClearance = landingClearance;
                    }
                    break;
                case GroundArrivalTaxiReplyIntent::IntentCode:
                    newClearance = dynamic_pointer_cast<GroundArrivalTaxiReplyIntent>(intent)->clearance();
                    break;
                }

                if (newClearance)
                {
                    flight()->addClearance(newClearance);
                }
            }
        }

        shared_ptr<Maneuver> maneuverFlightCycle()
        {
            time_t startTime = flight()->plan()->departureTime() - 180;
            time_t secondsBeforeStart = startTime - host()->getWorld()->currentTime();

            auto result = M.sequence(Maneuver::Type::Flight, "", { 
                M.delay(chrono::seconds(secondsBeforeStart)),
                maneuverDepartureAwaitIfrClearance(),
                maneuverDepartureAwaitPushback(),
                maneuverDeparturePushbackAndStart(),
                maneuverDepartureAwaitTaxi(),
                maneuverDepartureTaxi(),
                maneuverAwaitTakeOff(),
                maneuverTakeoff()
            });

            return result;
        }

        shared_ptr<Maneuver> maneuverFinalToGate(const Runway::End& landingRunway)
        {
            return M.sequence(Maneuver::Type::ArrivalApproach, "", {
                maneuverFinal(),
                maneuverLanding(),
                M.deferred([this]() {
                    return maneuverArrivalTaxiToGate();
                })
            });
        }

        shared_ptr<Maneuver> maneuverFinal()
        {
            auto aircraft = flight()->aircraft();
            auto flaps15GearDown = M.sequence(Maneuver::Type::Unspecified, "", {
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    0.15,
                    chrono::seconds(7),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setFlapState(value);
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    1.0,
                    chrono::seconds(10),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGearState(value);
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "",
                    -2.0,
                    0.0,
                    chrono::seconds(3),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress;
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                ))
            });
            auto flaps40 = M.parallel(Maneuver::Type::Unspecified, "", {
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "",
                    0.15,
                    0.4,
                    chrono::seconds(10),
                    [](const double &from, const double &to, double progress, double &value) {
                        value = from + (to - from) * progress;
                    },
                    [=](const double &value, double progress) {
                        aircraft->setFlapState(value);
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "",
                    0.0,
                    1.5,
                    chrono::seconds(5),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress;
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                ))
            });

            auto landingPoint = m_helper.getLandingPoint(flight());
            
            return M.sequence(Maneuver::Type::ArrivalApproach, "", {
                M.delay(chrono::seconds(10)),
                flaps15GearDown,
                M.tuneComRadio(flight(), m_helper.getArrivalTower(flight(), landingPoint)->frequency()),
                M.transmitIntent(flight(), I.pilotReportFinal(flight())),
                M.parallel(Maneuver::Type::Unspecified, "", {
                    M.sequence(Maneuver::Type::Unspecified, "", {
                        M.delay(chrono::seconds(20)),
                        flaps40,
                    }),
                    M.sequence(Maneuver::Type::Unspecified, "", {
                        M.awaitClearance(flight(), Clearance::Type::LandingClearance),
                        M.deferred([=](){
                            auto clearance = flight()->findClearanceOrThrow<LandingClearance>(Clearance::Type::LandingClearance);
                            auto readback = I.pilotLandingClearanceReadback(flight(), clearance, m_lastReceivedIntentId);
                            return M.transmitIntent(flight(), readback);
                        })
                    }),
                }),
            });
        }

        shared_ptr<Maneuver> maneuverLanding()
        {
            auto aircraft = flight()->aircraft();

            auto preFlare = M.parallel(Maneuver::Type::ArrivalLanding, "", {
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    1.5,
                    3.0,
                    chrono::milliseconds(3500),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    -1000,
                    -500,
                    chrono::milliseconds(3500),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setVerticalSpeedFpm(value);
                    }
                )),
            });
            auto flare = M.parallel(Maneuver::Type::ArrivalLanding, "", {
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    3.0,
                    5.5,
                    chrono::seconds(3),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "",
                    145,
                    135,
                    chrono::seconds(3),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress;
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGroundSpeedKt(value);
                    }
                )),
                M.sequence(Maneuver::Type::Unspecified, "", {
                    shared_ptr<Maneuver>(new AnimationManeuver<double>(
                        "",
                        -500,
                        -50,
                        chrono::seconds(2),
                        [](const double &from, const double &to, double progress, double &value) {
                            value = from + (to - from) * progress;
                        },
                        [=](const double &value, double progress) {
                            aircraft->setVerticalSpeedFpm(value);
                        }
                    )),
                    shared_ptr<Maneuver>(new AnimationManeuver<double>(
                        "",
                        -50,
                        -100,
                        chrono::seconds(1),
                        [](const double &from, const double &to, double progress, double &value) {
                            value = from + (to - from) * progress;
                        },
                        [=](const double &value, double progress) {
                            aircraft->setVerticalSpeedFpm(value);
                        }
                    ))
                })
            });
            auto touchDownAndDeccelerate = M.parallel(Maneuver::Type::ArrivalLandingRoll, "", {
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    1.0,
                    chrono::seconds(1),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setSpoilerState(value);
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    5.5,
                    0.0,
                    chrono::seconds(6),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                )),
                shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    135,
                    30,
                    chrono::seconds(20),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGroundSpeedKt(value);
                    }
                )),
            });

            return M.sequence(Maneuver::Type::ArrivalLanding, "", {
                M.await(Maneuver::Type::Unspecified, "", [=]() {
                    return (aircraft->altitude().type() == Altitude::Type::AGL && aircraft->altitude().feet() <= 55);
                }),
                preFlare,
                M.await(Maneuver::Type::Unspecified, "", [=]() {
                    return (aircraft->altitude().type() == Altitude::Type::AGL && aircraft->altitude().feet() <= 20);
                }),
                flare,
                M.await(Maneuver::Type::Unspecified, "", [=]() {
                    return (aircraft->altitude().type() == Altitude::Type::Ground);
                }),
                touchDownAndDeccelerate
            });
        }

        shared_ptr<Maneuver> maneuverArrivalTaxiToGate()
        {
            WorldHelper helper(host());
            auto aircraft = flight()->aircraft();
            auto airport = helper.getArrivalAirport(flight());
            auto runway = airport->getRunwayOrThrow(m_flightPlan->arrivalRunway());
            const auto& runwayEnd = runway->getEndOrThrow(m_flightPlan->arrivalRunway());
            auto gate = airport->getParkingStandOrThrow(m_flightPlan->arrivalGate());
            shared_ptr<TaxiEdge> exitFirstEdge;
            shared_ptr<TaxiEdge> exitLastEdge;
            string exitName;

            const auto safeCreateExitManeuver = [
                this, &exitFirstEdge, &exitLastEdge, &exitName, airport, runway, gate, aircraft, runwayEnd
            ]{
                shared_ptr<Maneuver> result;
                host()->writeLog(
                    "AIPILO|Flight[%s] landed rwy[%s] will look for exit path",
                    flight()->callSign().c_str(), runwayEnd.name().c_str());

                auto taxiPath = airport->taxiNet()->tryFindExitPathFromRunway(
                    runway,
                    runwayEnd,
                    gate,
                    aircraft->location());

                if (taxiPath)
                {
                    exitFirstEdge = taxiPath->edges[0];
                    exitLastEdge = taxiPath->edges[taxiPath->edges.size() - 1];
                    exitName = taxiPath->toHumanFriendlyString();

                    host()->writeLog(
                        "AIPILO|Flight[%s] arrival gate[%s] will exit runway[%s] via[%s]",
                        flight()->callSign().c_str(),
                        gate->name().c_str(),
                        runwayEnd.name().c_str(),
                        exitName.c_str());

                    result = M.taxiByPath(flight(), taxiPath, ManeuverFactory::TaxiType::HighSpeed);
                }
                else
                {
                    host()->writeLog(
                        "AIPILO|Flight[%s] arrival exit path from runway NOT FOUND! will teleport to gate[%s]",
                        flight()->callSign().c_str(),
                        gate->name().c_str());
                    result = M.instantAction([=]{
                        aircraft->park(gate);
                    });
                }

                return result;
            };

            auto flapsZero = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                "",
                0.4,
                0,
                chrono::seconds(30),
                [](const double& from, const double& to, double progress, double& value) {
                    value = from + (to - from) * progress;
                },
                [=](const double& value, double progress) {
                    aircraft->setFlapState(value);
                }
            ));
            auto speedBrakeDown = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                "",
                1.0,
                0,
                chrono::seconds(1),
                [](const double& from, const double& to, double progress, double& value) {
                    value = from + (to - from) * progress;
                },
                [=](const double& value, double progress) {
                    aircraft->setSpoilerState(value);
                }
            ));
            auto exitRunway = safeCreateExitManeuver();
            auto taxiLights = M.instantAction([=] {
               aircraft->setLights(Aircraft::LightBits::BeaconTaxiNav);
            });
            auto lightsOff = M.instantAction([=] {
                aircraft->setLights(Aircraft::LightBits::None);
            });

            const auto onHoldingShort = [=](shared_ptr<TaxiEdge> holdShortEdge) {
                return maneuverAwaitCrossRunway(airport, holdShortEdge);
            };

            return M.sequence(Maneuver::Type::ArrivalTaxi, "", {
                M.instantAction([aircraft] {
                    aircraft->setGroundSpeedKt(0);
                }),
                M.parallel(Maneuver::Type::Unspecified, "", {
                    flapsZero,
                    speedBrakeDown,
                    M.sequence(Maneuver::Type::Unspecified, "", {
                        M.await(Maneuver::Type::Unspecified, "", [this,exitFirstEdge]{
                            return (!exitFirstEdge) || isPointBehind(exitFirstEdge->node2()->location().geo());
                        }),
                        M.delay(chrono::seconds(3)),
                        M.tuneComRadio(flight(), airport->groundAt(aircraft->location())->frequency()),
                        M.transmitIntent(flight(), I.pilotArrivalCheckInWithGround(
                            flight(), runwayEnd.name(), exitName, exitLastEdge
                        )),
                        M.awaitClearance(flight(), Clearance::Type::ArrivalTaxiClearance),
                        M.deferred([this]{
                            auto clearance = flight()->findClearanceOrThrow<ArrivalTaxiClearance>(Clearance::Type::ArrivalTaxiClearance);
                            return M.transmitIntent(flight(), I.pilotArrivalTaxiReadback(flight(), m_lastReceivedIntentId));
                        }),
                    }),
                    M.sequence(Maneuver::Type::Unspecified, "", {
                       exitRunway,
                       taxiLights,
                       M.awaitClearance(flight(), Clearance::Type::ArrivalTaxiClearance),
                       M.parallel(Maneuver::Type::Unspecified, "", {
                           M.deferred([=]{
                               auto clearance = flight()->findClearanceOrThrow<ArrivalTaxiClearance>(Clearance::Type::ArrivalTaxiClearance);
                               return M.taxiByPath(
                                   flight(),
                                   clearance->taxiPath(),
                                   ManeuverFactory::TaxiType::Normal,
                                   onHoldingShort);
                           }),
                       }),
                   }),
                }),
                M.delay(chrono::seconds(5)),
                lightsOff
            });
        }

        shared_ptr<Maneuver> maneuverDepartureAwaitIfrClearance()
        {
            auto intentFactory = host()->services().get<IntentFactory>();
            auto ifrClearanceRequest = intentFactory->pilotIfrClearanceRequest(flight());
            auto airport = host()->getWorld()->getAirport(flight()->plan()->departureAirportIcao());
            auto clearanceDelivery = airport->clearanceDeliveryAt(flight()->aircraft()->location());
            auto ground = airport->groundAt(flight()->aircraft()->location());

            return M.sequence(Maneuver::Type::DepartureAwaitIfrClearance, "", {
                M.tuneComRadio(flight(), clearanceDelivery->frequency()),
                M.transmitIntent(flight(), ifrClearanceRequest),
                M.awaitClearance(flight(), Clearance::Type::IfrClearance),
                M.deferred([=]() {
                    host()->writeLog("ifrClearanceReadback deferred factory");
                    auto ifrClearanceReadback = intentFactory->pilotIfrClearanceReadback(flight(), m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), ifrClearanceReadback);
                }),
                M.await(Maneuver::Type::Unspecified, "", [=](){
                    return flight()->findClearanceOrThrow<IfrClearance>(Clearance::Type::IfrClearance)->readbackCorrect();
                }),
                M.deferred([=]() {
                    host()->writeLog("deliveryToGroundHandoffReadback deferred factory");
                    auto handoffReadback = intentFactory->pilotHandoffReadback(
                        flight(), clearanceDelivery, ground->frequency()->khz(), m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), handoffReadback);
                }),
                M.deferred([=]() {
                    return M.delay(chrono::seconds(5));
                })
            });
        }

        shared_ptr<Maneuver> maneuverDepartureAwaitPushback()
        {
            auto intentFactory = host()->services().get<IntentFactory>();
            auto pushAndStartRequest = intentFactory->pilotPushAndStartRequest(flight());
            auto airport = host()->getWorld()->getAirport(flight()->plan()->departureAirportIcao());
            auto ground = airport->groundAt(flight()->aircraft()->location());

            return M.sequence(Maneuver::Type::DepartureAwaitPushback, "", {
                M.tuneComRadio(flight(), ground->frequency()),
                M.transmitIntent(flight(), pushAndStartRequest),
                M.awaitClearance(flight(), Clearance::Type::PushAndStartApproval),
                M.deferred([=]() {
                    host()->writeLog("pushAndStartReadback deferred factory");
                    auto pushAndStartReadback =
                        intentFactory->pilotPushAndStartReadback(flight(), ground, m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), pushAndStartReadback);
                }),
                M.deferred([=]() {
                    return M.delay(chrono::seconds(5));
                })
            });
        }
        
        shared_ptr<Maneuver> maneuverDeparturePushbackAndStart()
        {
            const auto createPushbackTaxiPath = [this](const vector<GeoPoint>& pushbackPath) {
                vector<shared_ptr<TaxiEdge>> pushbackEdges;
                for (int i = 0 ; i < pushbackPath.size() - 1 ; i++)
                {
                    host()->writeLog(
                        "PUSHBACK-PATH (%.9f,%.9f)->(%.9f,%.9f)", 
                        pushbackPath[i].latitude,
                        pushbackPath[i].longitude,
                        pushbackPath[i+1].latitude,
                        pushbackPath[i+1].longitude);
                    pushbackEdges.push_back(shared_ptr<TaxiEdge>(new TaxiEdge(
                        UniPoint::fromGeo(host(), pushbackPath[i]),
                        UniPoint::fromGeo(host(), pushbackPath[i+1])
                    )));
                }
                auto taxiPath = shared_ptr<TaxiPath>(new TaxiPath(
                    pushbackEdges[0]->node1(),
                    pushbackEdges[pushbackEdges.size()-1]->node2(),
                    pushbackEdges
                ));
                return taxiPath;
            };

            return DeferredManeuver::create(Maneuver::Type::DeparturePushbackAndStart, "", [=]() {
                auto flightPlan = flight()->plan();
                auto approval = flight()->findClearanceOrThrow<PushAndStartApproval>(Clearance::Type::PushAndStartApproval);
                auto taxiPath = createPushbackTaxiPath(approval->pushbackPath());

                vector<shared_ptr<Maneuver>> maneuverSteps = {
                    M.switchLights(flight(), Aircraft::LightBits::Beacon),
                    M.delay(chrono::seconds(10)),
                    M.switchLights(flight(), Aircraft::LightBits::BeaconNav),
                    M.delay(chrono::seconds(5)),
                    M.taxiByPath(flight(), taxiPath, ManeuverFactory::TaxiType::Pushback)
                };

                return shared_ptr<Maneuver>(new SequentialManeuver(
                    Maneuver::Type::DeparturePushbackAndStart, 
                    "",
                    maneuverSteps
                )); 
            });
        }
        
        shared_ptr<Maneuver> maneuverDepartureAwaitTaxi()
        {
            auto flapsToTakeoffPosition = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                "", 
                0.0,
                0.15,
                chrono::seconds(3),
                [](const double& from, const double& to, double progress, double& value) {
                    value = from + (to - from) * progress; 
                },
                [=](const double& value, double progress) {
                    flight()->aircraft()->setFlapState(value);
                }
            ));

            auto intentFactory = host()->services().get<IntentFactory>();
            auto taxiRequest = intentFactory->pilotDepartureTaxiRequest(flight());

            return M.sequence(Maneuver::Type::DepartureAwaitTaxi, "", {
                M.delay(chrono::seconds(5)),
                flapsToTakeoffPosition,
                M.delay(chrono::seconds(5)),
                M.transmitIntent(flight(), taxiRequest),
                M.awaitClearance(flight(), Clearance::Type::DepartureTaxiClearance),
                M.deferred([=]() {
                    host()->writeLog("taxiReadback deferred factory");
                    auto taxiReadback = intentFactory->pilotDepartureTaxiReadback(flight(), m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), taxiReadback);
                }),
                M.deferred([=]() {
                    return M.delay(chrono::seconds(10));
                }),
            });
        }

        shared_ptr<Maneuver> maneuverDepartureTaxi()
        {
            const auto addLineupEdges = [=](shared_ptr<DepartureTaxiClearance> clearance) {
                auto taxiPath = clearance->taxiPath();
                auto runway = m_departureAirport->getRunwayOrThrow(clearance->departureRunway());
                const auto& runwayEnd = runway->getEndOrThrow(clearance->departureRunway());

                auto centerlinePoint = taxiPath->toNode->location().geo();
                auto lineupPoint1 = GeoMath::getPointAtDistance(
                    centerlinePoint,
                    runwayEnd.heading(),
                    30);
                auto lineupPoint2 = GeoMath::getPointAtDistance(
                    centerlinePoint,
                    runwayEnd.heading(),
                    60);

                taxiPath->appendEdgeTo(UniPoint::fromGeo(host(), lineupPoint1));
                taxiPath->appendEdgeTo(UniPoint::fromGeo(host(), lineupPoint2));
            };

            const auto onHoldingShort = [=](shared_ptr<TaxiEdge> holdShortEdge) {
                auto departureRunway = m_departureAirport->getRunwayOrThrow(m_flightPlan->departureRunway());
                bool isHoldingShortDepartureRunway = holdShortEdge->activeZones().departue.has(departureRunway);
                shared_ptr<Maneuver> holdShortManeuver = isHoldingShortDepartureRunway
                    ? maneuverDepartureAwaitLineup(m_flightPlan->departureRunway(), holdShortEdge)
                    : maneuverAwaitCrossRunway(m_departureAirport, holdShortEdge);
                return holdShortManeuver;
            };

            return DeferredManeuver::create(Maneuver::Type::DepartureTaxi, "", [=]() {
                auto clearance = flight()->findClearanceOrThrow<DepartureTaxiClearance>(Clearance::Type::DepartureTaxiClearance);
                addLineupEdges(clearance);

                vector<shared_ptr<Maneuver>> steps;
                steps.push_back(M.delay(chrono::seconds(10)));
                steps.push_back(M.switchLights(flight(), Aircraft::LightBits::BeaconTaxi));
                steps.push_back(M.delay(chrono::seconds(5)));
                steps.push_back(M.taxiByPath(
                    flight(), 
                    clearance->taxiPath(), 
                    ManeuverFactory::TaxiType::Normal,
                    onHoldingShort));

                return shared_ptr<Maneuver>(new SequentialManeuver(
                    Maneuver::Type::DepartureTaxi,
                    "",
                    steps
                ));
            });
        }

        shared_ptr<Maneuver> maneuverDepartureAwaitLineup(const string& runwayName, shared_ptr<TaxiEdge> holdShortEdge)
        {
            return M.sequence(Maneuver::Type::DepartureLineUpAndWait, "", {
                M.transmitIntent(flight(), I.pilotReportHoldingShort(flight(), runwayName, holdShortEdge->name())),
                M.await(Maneuver::Type::Unspecified, "", [this](){
                    return (m_departureTowerKhz > 0);
                }),
                M.deferred([=]() {
                    auto ground = m_departureAirport->groundAt(flight()->aircraft()->location());
                    return M.transmitIntent(flight(), I.pilotHandoffReadback(flight(), ground, m_departureTowerKhz, m_lastReceivedIntentId));
                }),
                M.instantAction([=]() {
                    aircraft()->setFrequencyKhz(m_departureTowerKhz);
                }),
                M.transmitIntent(flight(), I.pilotCheckInWithTower(flight(), /*flight()->plan()->departureRunway()*/"", holdShortEdge->name(), false)),
                M.awaitClearance(flight(), Clearance::Type::LineupApproval),
                M.deferred([=]() {
                    auto approval = flight()->findClearanceOrThrow<LineupApproval>(Clearance::Type::LineupApproval);
                    auto readback = I.pilotLineUpReadback(approval, m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), readback);
                }),
                M.instantAction([=]() {
                    flight()->aircraft()->setLights(Aircraft::LightBits::BeaconLandingNavStrobe);
                }),
                M.delay(chrono::seconds(5)),
            });
        }

        shared_ptr<Maneuver> maneuverAwaitCrossRunway(shared_ptr<Airport> airport, shared_ptr<TaxiEdge> holdShortEdge)
        {
            auto runway = getActiveZoneRunway(airport, holdShortEdge);
            string runwayName = runway ? runway->end1().name() : "";

            return M.sequence(Maneuver::Type::TaxiHoldShort, "", {
                M.transmitIntent(flight(), I.pilotReportHoldingShort(flight(), runwayName, holdShortEdge->name())),
                M.awaitClearance(flight(), Clearance::Type::RunwayCrossClearance),
                M.deferred([=]() {
                    auto clearance = flight()->findClearanceOrThrow<RunwayCrossClearance>(Clearance::Type::RunwayCrossClearance);
                    return M.transmitIntent(flight(), I.pilotAffirmation(flight(), clearance->header().issuedBy, m_lastReceivedIntentId));
                })
            });
        }

        shared_ptr<Maneuver> maneuverAwaitTakeOff()
        {
            return M.sequence(Maneuver::Type::DepartureAwaitTakeOff, "", {
                M.awaitClearance(flight(), Clearance::Type::TakeoffClearance),
                M.deferred([=]() {
                    auto clearance = flight()->findClearanceOrThrow<TakeoffClearance>(Clearance::Type::TakeoffClearance);
                    auto readback = I.pilotTakeoffClearanceReadback(flight(), clearance, m_departureKhz, m_lastReceivedIntentId);
                    return M.transmitIntent(flight(), readback);
                }),
                M.delay(chrono::seconds(5)),
            });
        }        

        shared_ptr<Maneuver> maneuverTakeoff()
        {
            return DeferredManeuver::create(Maneuver::Type::DepartureTakeOffRoll, "", [=]() {
                auto clearance = flight()->findClearanceOrThrow<TakeoffClearance>(Clearance::Type::TakeoffClearance);
                auto aircraft = flight()->aircraft();
                auto runway = m_departureAirport->getRunwayOrThrow(clearance->departureRunway());
                const auto& runwayEnd = runway->getEndOrThrow(clearance->departureRunway());
                float runwayHeading = runwayEnd.heading();

                auto rollOnRunway = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    140.0,
                    chrono::seconds(20),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGroundSpeedKt(value);
                    }
                ));
                auto rotate1 = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    8.5,
                    chrono::seconds(3),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                ));
                auto rotate2 = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    8.5,
                    15.0,
                    chrono::seconds(6),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setAttitude(aircraft->attitude().withPitch(value));
                    }
                ));
                auto liftUp = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    0,
                    2500.0,
                    chrono::seconds(10),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setVerticalSpeedFpm(value);
                        //aircraft->setAltitude(value);
                    }
                ));
                auto gearUp = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    1.0,
                    0.0,
                    chrono::seconds(8),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGearState(value);
                    }
                ));
                auto accelerateAirborne = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    140.0,
                    180.0,
                    chrono::seconds(30),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGroundSpeedKt(value);
                    }
                ));
                auto turnToInitialHeading = shared_ptr<Maneuver>(new AnimationManeuver<double>(
                    "", 
                    140.0,
                    210.0,
                    chrono::seconds(30),
                    [](const double& from, const double& to, double progress, double& value) {
                        value = from + (to - from) * progress; 
                    },
                    [=](const double& value, double progress) {
                        aircraft->setGroundSpeedKt(value);
                    }
                ));

                return M.sequence(Maneuver::Type::Unspecified, "", {
                    M.instantAction([=]() {
                        const auto& runwayEnd = runway->getEndOrThrow(m_flightPlan->departureRunway());
                        aircraft->setAttitude(aircraft->attitude().withHeading(runwayEnd.heading()));
                    }),
                    M.parallel(Maneuver::Type::Unspecified, "", {
                        M.sequence(Maneuver::Type::Unspecified, "", {
                            rollOnRunway,
                            accelerateAirborne,
                        }),
                        M.sequence(Maneuver::Type::Unspecified, "", {
                            M.delay(chrono::seconds(20)),
                            rotate1,
                            rotate2,
                        }),
                        M.sequence(Maneuver::Type::Unspecified, "", {
                            M.delay(chrono::seconds(23)),
                            liftUp
                        }),
                        M.sequence(Maneuver::Type::Unspecified, "", {
                            M.delay(chrono::seconds(25)),
                            gearUp,
                        }),
                        M.sequence(Maneuver::Type::Unspecified, "", {
                            M.delay(chrono::seconds(32)),
                            M.airborneTurn(flight(), runwayHeading, clearance->initialHeading()),
                        }),
                    })
                });
            });
        }

        shared_ptr<Runway> getActiveZoneRunway(shared_ptr<Airport> airport, shared_ptr<TaxiEdge> activeZoneEdge)
        {
            host()->writeLog(
                "AIPILO|getActiveZoneRunway: edge [%d|%s] departure-mask [%d] arrival-mask [%d] ils-mask [%d]",
                activeZoneEdge->id(),
                activeZoneEdge->name().c_str(),
                activeZoneEdge->activeZones().departue.runwaysMask(),
                activeZoneEdge->activeZones().arrival.runwaysMask(),
                activeZoneEdge->activeZones().ils.runwaysMask());

            for (const auto& runway : airport->runways())
            {
                host()->writeLog(
                    "AIPILO|getActiveZoneRunway: checking runway [%s/%s], maskbit [%d]",
                    runway->end1().name().c_str(), runway->end2().name().c_str(), runway->maskBit());

                if (activeZoneEdge->activeZones().departue.has(runway) ||
                    activeZoneEdge->activeZones().arrival.has(runway) ||
                    activeZoneEdge->activeZones().ils.has(runway))
                {
                    host()->writeLog("AIPILO|getActiveZoneRunway: FOUND");
                    return runway;//runwayName = runway->end1().name();
                }
            }

            host()->writeLog("AIPILO|getActiveZoneRunway: RUNWAY NOT FOUND");
            return nullptr;
        }

        bool isPointBehind(const GeoPoint& point)
        {
            float headingToPoint = GeoMath::getHeadingFromPoints(aircraft()->location(), point);
            float turnToNodeDegrees = GeoMath::getTurnDegrees(aircraft()->attitude().heading(), headingToPoint);
            return (abs(turnToNodeDegrees) >= 45);
        }
    };
}

