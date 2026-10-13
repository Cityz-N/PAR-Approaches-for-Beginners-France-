PAR - Precision Approach Radar scopes for EuroScope (France)  -  version 0.2.0

WHAT YOU GET
  A floating window (movable, resizable, can go on another monitor) with
    - AZIMUTH scope (right side, vertical): top view, threshold at the top, range at the bottom,
      aircraft left/right of the extended centreline in metres (right of centreline = right)
    - ELEVATION scope (bottom left): side view, aircraft above/below the glidepath, in feet
    - an info panel (top left, above the elevation scope): distance, groundspeed, QNH altitude, deviations, heading to fly,
      decision-height warning and a SUGGESTED TRANSMISSION in PAR phraseology, e.g.
      "3.2 miles from touchdown. Slightly right of centreline, turn left 2 degrees,
       heading 072. Slightly above glidepath, increase rate of descent, about 650 feet per minute."
  Elevation reads left to right (range on the left, THR on the right); azimuth reads bottom to top.
  Amber dashed lines = course limits (+/-1 deg azimuth, +/-0.5 deg elevation).
  The window follows the aircraft SELECTED in EuroScope (yellow, with a short trail);
  other aircraft in the approach cone can be shown dimmed with their callsign (.par others on).

COMMANDS
  .par rwy LFRJ 07       choose airfield + runway (opens the window)
  .par list              list the runways of PAR_Runways.csv
  .par show | hide       show / hide the window (the X of the window only hides it)
  .par track CALLSIGN    lock on one aircraft;   .par track asel = follow the selection again
  DESELECT AIRCRAFT button (top right of the info panel): release the tracked aircraft; selecting another aircraft in EuroScope
                         tracks it again (re-selecting the same one: .par track asel)
  .par clear             stop tracking (= .par track off), nothing is shown until .par track ...
  .par others on|off     show / hide the other traffic in the approach cone (default off, INI ShowOthers)
  .par range 10          range in NM       .par az 500    azimuth half-height, metres
  .par el 500            elevation half-height, ft         .par qnh 1015     live QNH
  .par dh 200            decision height, ft               .par top on|off
  .par reload            re-read PAR.ini and PAR_Runways.csv           .par status

RUNWAYS INCLUDED (PAR_Runways.csv: 24 real PAR runway ends, then 37 "just for fun" runways)
  LFRJ Landivisiau 07 25   LFSX Luxeuil 29        LFOA Avord 06 24       LFMI Istres 15 33
  LFBC Cazaux 06 24        LFOJ Orleans 07 25     LFSI St-Dizier 11      LFBM Mont-de-Marsan 09 27
  LFBG Cognac 05 23        LFTH Hyeres 05         LFRL Lanveoc 23        LFRH Lorient 07 20 25
  LFKS Solenzara 18        LFMO Orange 14 32
  Fun runways (taken from ILS window config; no real PAR there, RDH assumed 50 ft):
  LFML LFPG LFLL LFBO LFPO LFRS LFBD LFMT LFMN LFSB LFBZ LFBH LFRB LFPB LFOB LFQQ LFRN
  Not checked / not found: LFSO Nancy-Ochey, LFTF Cuers, LFYR Romorantin, LFBY Dax ("PAR" is
  mentioned on its chart but no PAR procedure was found), LFPV Villacoublay, LFBD Bordeaux.
  To add a runway, add a line to the CSV (format explained at its top).

THINGS TO KNOW
  - Data come from the public DIRCAM MIAC2 airfield documents (PAR charts), read by a text
    extraction: coordinates, elevations, courses, glidepath and RDH should be verified before
    serious use. Where the document gives no true course, TRUE = MAG + 1 deg was assumed (marked
    in the NOTE column); courses of runways with two thresholds were computed from the coordinates.
  - The reference point is the threshold (a displaced threshold where the chart gives that one).
    Touchdown is taken 300 m after it for the "miles from touchdown" call.
  - Altitude = pressure altitude + (QNH - 1013) x 27 ft: remember to set .par qnh.
  - Suggested corrections use geometry only (no wind); the cross-track speed of the last few
    seconds is used to say "converging, maintain heading" instead of another turn.
  - The real PAR display uses a magnified scale. Use .par az / .par el to zoom in the same way.
  - Realistic PAR coverage limits and radar sweep are not simulated: radar updates follow the
    VATSIM position updates EuroScope receives (about every 5 s).
  - First version, not yet tested inside EuroScope: expect a round of fixes.


=====================================
PAR PHRASEOLOGY  /  PHRASEOLOGIE PAR
=====================================
Based on the ICAO precision radar approach phraseology (Doc 4444 / Doc 9432) as I know it, with
a French rendering following the usual French military/civil usage. Wording varies a little
between units: check it against your own reference (French RCA3 / MIAC documents) before
using it for real. The "Suggested transmission" of the plugin follows the English column.

1. START OF THE APPROACH / DEBUT DE L'APPROCHE
   EN  This will be a precision radar approach runway 24.
   FR  Ceci sera une approche de précision radar piste 24.
   EN  Do not acknowledge further transmissions.            (the pilot stays silent from now on)
   FR  N'accusez plus réception des transmissions.  /  Ne répondez plus.
   EN  Complete landing checks.  Report gear down and locked.
   FR  Effectuez vos vérifications avant atterrissage.  Annoncez train sorti verrouillé.
   EN  Report established on final / Radar contact, [distance] miles from touchdown.
   FR  Contact radar, à [distance] milles du point de poser.
   EN  Transmissions will be on frequency [..] / I will call you every [..] seconds.
   FR  Les transmissions seront faites sur [..] / Je vous appellerai toutes les [..] secondes.
   EN  Check minima / Decision height [..] ft.
   FR  Vérifiez vos minima. Hauteur de décision [..] pieds.

2. AZIMUTH (CENTRELINE) / AZIMUT (AXE)
   EN  On centreline.                                   FR  Dans l'axe.  /  Sur l'axe.
   EN  Slightly left of centreline.                     FR  Légèrement à gauche de l'axe.
   EN  Slightly right of centreline.                    FR  Légèrement à droite de l'axe.
   EN  Left of centreline.                              FR  À gauche de l'axe.
   EN  Right of centreline.                             FR  À droite de l'axe.
   EN  Well left of centreline.                         FR  Bien à gauche de l'axe.
   EN  Well right of centreline.                        FR  Bien à droite de l'axe.
   EN  Going left of centreline.                        FR  Vous vous écartez à gauche de l'axe.
   EN  Going right of centreline.                       FR  Vous vous écartez à droite de l'axe.
   EN  Turn left heading 245.                           FR  Virez à gauche cap 245.
   EN  Turn right heading 250.                          FR  Virez à droite cap 250.
   EN  Turn left 3 degrees, heading 245.                FR  Virez à gauche de 3 degrés, cap 245.
   EN  Heading is good / Maintain present heading.      FR  Cap correct.  /  Maintenez le cap.
   EN  Heading 247, on centreline.                      FR  Cap 247, dans l'axe.
   EN  Converging centreline, maintain heading.         FR  Vous revenez sur l'axe, maintenez le cap.

3. ELEVATION (GLIDEPATH) / SITE (PLAN DE DESCENTE)
   EN  Approaching glidepath.                           FR  Vous approchez du plan.
   EN  Begin descent now.  /  Commence descent now.     FR  Commencez la descente maintenant.
   EN  On glidepath.                                    FR  Dans le plan.
   EN  Slightly above glidepath.                        FR  Légèrement au-dessus du plan.
   EN  Slightly below glidepath.                        FR  Légèrement au-dessous du plan.
   EN  Above glidepath.                                 FR  Au-dessus du plan.
   EN  Below glidepath.                                 FR  Au-dessous du plan.
   EN  Well above glidepath.                            FR  Bien au-dessus du plan.
   EN  Well below glidepath.                            FR  Bien au-dessous du plan.
   EN  Going above glidepath.                           FR  Vous montez au-dessus du plan.
   EN  Going below glidepath.                           FR  Vous passez sous le plan.
   EN  Increase rate of descent.                        FR  Augmentez votre taux de descente.
   EN  Reduce rate of descent.                          FR  Réduisez votre taux de descente.
   EN  Adjust rate of descent / Rate of descent is good.  FR  Ajustez votre taux de descente / Taux de descente correct.
   EN  Level off, check altitude.                       FR  Mettez en palier, vérifiez votre altitude.
   EN  Check gear down and locked.                      FR  Vérifiez train sorti verrouillé.

4. DISTANCE
   EN  [5] miles from touchdown.                        FR  À [5] milles du point de poser.
   EN  [1] mile from touchdown.                         FR  À [1] mille du point de poser.
   EN  Half a mile from touchdown.                      FR  À un demi-mille du point de poser.
   (given every mile at first, then every half mile, then every ~1/4 mile on short final)

5. FINAL, MINIMA, LANDING / FINALE, MINIMA, ATTERRISSAGE
   EN  Over the approach lights.                        FR  Au-dessus des feux d'approche.
   EN  Over the threshold.                              FR  Au seuil de piste.
   EN  Approaching decision height.                     FR  Vous approchez de la hauteur de décision.
   EN  At decision height.  /  Decision height.         FR  Hauteur de décision.
   EN  If runway in sight, continue visually and land.  FR  Si piste en vue, poursuivez à vue et atterrissez.
   EN  Runway in sight? / Report runway in sight.       FR  Piste en vue ?  /  Annoncez piste en vue.
   EN  Cleared to land runway 24, wind 240 degrees 8 knots.
   FR  Autorisé à atterrir piste 24, vent 240 degrés 8 nœuds.
   EN  Do not acknowledge, cleared to land.             FR  N'accusez pas réception, autorisé à atterrir.
   EN  Radar service terminated, contact tower [..].    FR  Fin du service radar, contactez la tour [..].

6. GO-AROUND AND PROBLEMS / REMISE DE GAZ ET ANOMALIES
   EN  Go around.  (then the missed-approach instructions)       FR  Remettez les gaz.  (puis consignes d'approche interrompue)
   EN  Go around, I say again, go around, you are well [left/below..].
   FR  Remettez les gaz, je répète, remettez les gaz, vous êtes bien [à gauche/au-dessous..].
   EN  Missed approach: climb straight ahead to [..], then [..].
   FR  Approche interrompue : montez dans l'axe jusqu'à [..], puis [..].
   EN  Radar contact lost, if runway not in sight go around.
   FR  Contact radar perdu, si piste pas en vue remettez les gaz.
   EN  Unable to continue the approach (reason).        FR  Impossible de poursuivre l'approche (raison).
   EN  Request repeat / Say again.                      FR  Répétez.
   EN  I am unable to monitor the approach (equipment failure).  FR  Je ne peux plus surveiller l'approche (panne matériel).
   EN  Pilot: Request PAR / Request precision radar approach runway 24.
   FR  Pilote : Demande approche de précision radar piste 24.

7. EXAMPLE OF A FULL TALK-DOWN / EXEMPLE
   EN  "This will be a precision radar approach runway 24. Do not acknowledge further transmissions.
        Turn right heading 245. [..] Eight miles from touchdown. Slightly right of centreline,
        turn left 2 degrees, heading 243. Approaching glidepath. Begin descent now. Seven miles from
        touchdown. On centreline, on glidepath. Check gear down and locked. Six miles from touchdown,
        on centreline, slightly above glidepath, increase rate of descent. [..] One mile from touchdown,
        on centreline, on glidepath. Over the approach lights. Decision height. If runway in sight,
        continue visually and land. Cleared to land runway 24, wind 240 degrees 8 knots."
   FR  "Ceci sera une approche de précision radar piste 24. N'accusez plus réception. Virez à droite cap 245.
        [..] À huit milles du point de poser. Légèrement à droite de l'axe, virez à gauche de 2 degrés,
        cap 243. Vous approchez du plan. Commencez la descente maintenant. À sept milles du point de
        poser. Dans l'axe, dans le plan. Vérifiez train sorti verrouillé. À six milles, dans l'axe,
        légèrement au-dessus du plan, augmentez votre taux de descente. [..] À un mille, dans l'axe,
        dans le plan. Au-dessus des feux d'approche. Hauteur de décision. Si piste en vue, poursuivez
        à vue et atterrissez. Autorisé à atterrir piste 24, vent 240 degrés 8 nœuds."
