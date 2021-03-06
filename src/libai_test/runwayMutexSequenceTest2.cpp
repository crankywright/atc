//
// This file is part of AT&C project which simulates virtual world of air traffic and ATC.
// Code licensing terms are available at https://github.com/felix-b/atc/blob/master/LICENSE
//
#include <chrono>
#include <memory>
#include <functional>
#include "gtest/gtest.h"
#include "libworld.h"
#include "clearanceTypes.hpp"
#include "simpleRunwayMutex.hpp"
#include "libworld_test.h"
#include "mutexTestCase.hpp"
#include "mutexLongRunningTestCase.hpp"

using namespace std;
using namespace world;
using namespace ai;

TEST(RunwayMutexTest, longRun_crossing)
{
    MutexLongRunningTestCase t;

    t.COL_ARRIVAL(101, "B738");
    t.COL_DEPARTURE(102, "A320");
    t.COL_DEPARTURE(103, "B747");
    t.COL_DEPARTURE(104, "A319");
    t.COL_DEPARTURE(105, "B777");
    t.COL_DEPARTURE(106, "A380");

    const auto& C = t.CELL;

    t.ROW(  0, { C.FIN(150)                , C.GND_GATE()              , C.FIN(390)                , C.FIN(400)                , C.GND_GATE()              , C.FIN(500)                });
    t.ROW(  5, { C.FIN(145)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 10, { C.FIN(140)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 15, { C.FIN(135)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 20, { C.FIN(130)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 25, { C.FIN(125)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 30, { C.FIN_CHK_CONT(120)       , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 35, { C.FIN(115)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 40, { C.FIN(110)                , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 45, { C.FIN(105)                , C.HSCRS_CLR(C.TA_FIN("B738",4)),C.NOTHING()           , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 50, { C.FIN(100)                , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 55, { C.FIN(95)                 , C.CROSSING()              , C.HSCRS_CLR(C.TA_FIN("B738",3)),C.NOTHING()           , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 60, { C.FIN(90)                 , C.CROSSING()              , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 65, { C.FIN(85)                 , C.CROSSING()              , C.CROSSING()              , C.HSCRS_HLD_LND()         , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 70, { C.FIN(80)                 , C.CRS_VACATED()           , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 75, { C.FIN(75)                 , C.NOTHING()               , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 80, { C.FIN_CLR(70)             , C.NOTHING()               , C.CRS_VACATED()           , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 85, { C.FIN(65)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 90, { C.FIN(60)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });
    t.ROW( 95, { C.FIN(55)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               });

    t.ROW(100, { C.FIN(50)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(105, { C.FIN(45)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(110, { C.FIN(40)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(115, { C.FIN(35)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(120, { C.FIN(30)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(125, { C.FIN(25)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.HS(1)                   , C.NOTHING()                });
    t.ROW(130, { C.FIN(20)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.HS_CHK_HLD_DRLND(1,1)   , C.NOTHING()                });
    t.ROW(135, { C.FIN(15)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(140, { C.FIN(10)                 , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(145, { C.FIN(5)                  , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(150, { C.TOUCHDN()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.LUAW()                  , C.NOTHING()                });
    t.ROW(155, { C.LND_ROLL(1)             , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(160, { C.LND_ROLL(2)             , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(165, { C.LND_ROLL(3)             , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.LINEDUP()               , C.NOTHING()                });
    t.ROW(170, { C.LND_ROLL(4)             , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.LINEDUP()               , C.HSCRS_HLD_LND()          });
    t.ROW(175, { C.LND_ROLL(5)             , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.LINEDUP()               , C.NOTHING()                });
    t.ROW(180, { C.LND_VACATED()           , C.NOTHING()               , C.NOTHING()               , C.CLR_CRS(true)           , C.LINEDUP()               , C.CLR_CRS(true)            });
    t.ROW(185, { C.GND_GATE()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.LINEDUP()               , C.NOTHING()                });
    t.ROW(190, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.CROSSING()              , C.LINEDUP()               , C.CROSSING()               });
    t.ROW(195, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.CROSSING()              , C.LINEDUP()               , C.CRS_VACATED()            });

    t.ROW(200, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.CROSSING()              , C.LINEDUP()               , C.NOTHING()                });
    t.ROW(205, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.CRS_VACATED()           , C.LUAW_CLRTO()            , C.NOTHING()                });
    t.ROW(210, { C.NOTHING()               , C.HSCRS_HLD_DEP()         , C.NOTHING()               , C.NOTHING()               , C.TO_ROLL(1)              , C.NOTHING()                });
    t.ROW(215, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.TO_ROLL(2)              , C.NOTHING()                });
    t.ROW(220, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.TO_ROLL(3)              , C.NOTHING()                });
    t.ROW(225, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.TO_ROLL(4)              , C.NOTHING()                });
    t.ROW(230, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.TO_ROLL(5)              , C.NOTHING()                });
    t.ROW(235, { C.NOTHING()               , C.CLR_CRS(false)          , C.NOTHING()               , C.NOTHING()               , C.TO_VACATED()            , C.NOTHING()                });
    t.ROW(240, { C.NOTHING()               , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(245, { C.NOTHING()               , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(250, { C.NOTHING()               , C.CROSSING()              , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(255, { C.NOTHING()               , C.CRS_VACATED()           , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(260, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(265, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(270, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(275, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(280, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(285, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(290, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(295, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });

    t.ROW(300, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(305, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(310, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(315, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(320, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(325, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(330, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(335, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(340, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(345, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(350, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(355, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(360, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(365, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(370, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(375, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(380, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(385, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(390, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(395, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });

    t.ROW(400, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(405, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(410, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(415, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(420, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(425, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(430, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(435, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(440, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(445, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(450, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(455, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(460, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(465, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(470, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(475, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(480, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(485, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(490, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });
    t.ROW(495, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });

    t.ROW(500, { C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()               , C.NOTHING()                });

    //    EXPECT_TRUE(
    //        t.run(0, 500, 5)
    //    );
}
