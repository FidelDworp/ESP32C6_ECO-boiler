# ESP32C6_ECO-boiler

Onze zonneboiler op het dak oogst zonne energie tijdens zonnige dagen en haard-energie in de winterse periode. (Zowel onze zoon als wij hebben een zelfde type haard van JIDE, die een deel van de warmte naar de boiler sturen met hun eigen pomp en sensoren.)

De zonneboiler, heeft enkel deze elementen:
- Een PT1000 sensor (in de collector) om de temperatuur aan de "warme" zijde te meten.
- Een 230V pomprelais en een 5V PWM signaal (0-255) om de snelheid van de pomp te varieren.
- Een OEG pomp, PWM gestuurd.
- Een controller (van het duitse OEG). Die was echter niet intuitief in gebruik en hij werkte in sommige gevallen niet efficient noch betrouwbaar! 

Ik besloot deze controller dan maar door een eigen ontwerp te vervangen.
Voor onze verlichting en HVAC gebruikten we toen al Particle Photons (gemonteerd op mijn eigen ontwerp van shield) met succes.

Dus maakte ik een box met een Photon shield en deze peripherals:
- De bestaande PT1000 sensor (in de collector) om de temperatuur aan de "warme" zijde te meten.
- De bestaande OEG pomp, PWM gestuurd.
- Een bordje om de PT1000 sensor op het dak te lezen en aan de Photon door te geven.
- Een 5V pomprelais, aangesloten op de Photon.
- Een PWM signaal van de Photon, aangesloten op de OEG pomp.

Deze controller heeft tot nu toe reeds 5 jaar goeie dienst bewezen, maar een groot nadeel is de wifi stabiliteit van de Photon. De controller verliest te dikwijls "de pedalen" zodat de zonneboiler soms niet meer doorpompt wordt...

Dit deed me besluiten om ook dit systeem om te vormen tot een ESP32C6 controller, die een véél modernere WiFi module heeft, en heel wat krachtiger tegelijk.

FiDel, Recht 11jan26

----------------------------

