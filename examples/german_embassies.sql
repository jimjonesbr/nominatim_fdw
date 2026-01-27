/*
 * == Examples ==
 * nominatim_reverse() - Find addresses for given geo coordinates
 * nominatim_search() - Search coordinates for given addresses
 * Requires PostGIS
 *
 * DROP SERVER IF EXISTS osm;
 * DROP TABLE IF EXISTS public.german_embassy;
 */

CREATE EXTENSION IF NOT EXISTS nominatim_fdw;
CREATE EXTENSION IF NOT EXISTS postgis;

CREATE SERVER IF NOT EXISTS osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

CREATE TABLE IF NOT EXISTS public.german_embassy (
  id int GENERATED ALWAYS AS IDENTITY,
  country text,
  wikidataid text,
  city text,
  geom geometry(point,4326),
  address text,
  distance numeric
);

INSERT INTO public.german_embassy (country, wikidataid, city, geom)
VALUES
(E'Afghanistan', 'http://www.wikidata.org/entity/Q889',E'Kabul District', ST_MakePoint(69.180307,34.5319485)),
(E'Albania', 'http://www.wikidata.org/entity/Q222',E'Tirana', ST_MakePoint(19.807508333,41.329397222)),
(E'Algeria', 'http://www.wikidata.org/entity/Q262',E'Algiers Province', ST_MakePoint(3.0435,36.76577)),
(E'Angola', 'http://www.wikidata.org/entity/Q916',E'Luanda', ST_MakePoint(13.232778,-8.81)),
(E'Argentina', 'http://www.wikidata.org/entity/Q414',E'Buenos Aires', ST_MakePoint(-58.4396,-34.5673)),
(E'Armenia', 'http://www.wikidata.org/entity/Q399',E'Kentron District', ST_MakePoint(44.528,40.1798)),
(E'Australia', 'http://www.wikidata.org/entity/Q408',E'Canberra', ST_MakePoint(149.1122,-35.308)),
(E'Austria', 'http://www.wikidata.org/entity/Q40',E'Vienna', ST_MakePoint(16.382778,48.197778)),
(E'Azerbaijan', 'http://www.wikidata.org/entity/Q227',E'Baku', ST_MakePoint(49.84494,40.37423)),
(E'Bahrain', 'http://www.wikidata.org/entity/Q398',E'Capital Governorate', ST_MakePoint(50.5817195,26.219761)),
(E'Bangladesh', 'http://www.wikidata.org/entity/Q902',E'Dhaka District', ST_MakePoint(90.4223,23.7979)),
(E'Belarus', 'http://www.wikidata.org/entity/Q184',E'Partyzanski District', ST_MakePoint(27.576585,53.907067)),
(E'Belgium', 'http://www.wikidata.org/entity/Q31',E'Brussels', ST_MakePoint(4.3738,50.8439)),
(E'Benin', 'http://www.wikidata.org/entity/Q962',E'Cotonou', ST_MakePoint(2.4211,6.3529)),
(E'Bolivia', 'http://www.wikidata.org/entity/Q750',E'La Paz', ST_MakePoint(-68.12534,-16.50828)),
(E'Bosnia and Herzegovina', 'http://www.wikidata.org/entity/Q225',E'Sarajevo', ST_MakePoint(18.41595,43.8553)),
(E'Botswana', 'http://www.wikidata.org/entity/Q963',E'South-East District', ST_MakePoint(25.9135535,-24.6579622)),
(E'Brazil', 'http://www.wikidata.org/entity/Q155',E'Brasília', ST_MakePoint(-47.8863,-15.8213)),
(E'Brunei', 'http://www.wikidata.org/entity/Q921',E'Bandar Seri Begawan', ST_MakePoint(114.94085,4.88755)),
(E'Bulgaria', 'http://www.wikidata.org/entity/Q219',E'Sofia', ST_MakePoint(23.3493,42.67)),
(E'Burkina Faso', 'http://www.wikidata.org/entity/Q965',E'Ouagadougou Department', ST_MakePoint(-1.5076267,12.3786266)),
(E'Burundi', 'http://www.wikidata.org/entity/Q967',E'Bujumbura', ST_MakePoint(29.36016,-3.38486)),
(E'Cambodia', 'http://www.wikidata.org/entity/Q424',E'Phnom Penh', ST_MakePoint(104.919507,11.559627)),
(E'Cameroon', 'http://www.wikidata.org/entity/Q1009',E'Centre', ST_MakePoint(11.5091,3.8859)),
(E'Canada', 'http://www.wikidata.org/entity/Q16',E'Ottawa', ST_MakePoint(-75.6818,45.4185)),
(E'Chad', 'http://www.wikidata.org/entity/Q657',E'N\'Djamena', ST_MakePoint(15.050217,12.100735)),
(E'Chile', 'http://www.wikidata.org/entity/Q298',E'Vitacura', ST_MakePoint(-70.5755,-33.39668)),
(E'Colombia', 'http://www.wikidata.org/entity/Q739',E'Bogotá', ST_MakePoint(-74.038797,4.6907022)),
(E'Costa Rica', 'http://www.wikidata.org/entity/Q800',E'San José', ST_MakePoint(-84.1065299,9.9392312)),
(E'Croatia', 'http://www.wikidata.org/entity/Q224',E'Zagreb', ST_MakePoint(15.96934,45.79917)),
(E'Cuba', 'http://www.wikidata.org/entity/Q241',E'Plaza de la Libertad', ST_MakePoint(-82.396587,23.137092)),
(E'Cyprus', 'http://www.wikidata.org/entity/Q229',E'Nicosia District', ST_MakePoint(33.352222222,35.159722222)),
(E'Czech Republic', 'http://www.wikidata.org/entity/Q213',E'Prague', ST_MakePoint(14.39815,50.087064)),
(E'Democratic Republic of the Congo', 'http://www.wikidata.org/entity/Q974',E'Gombe', ST_MakePoint(15.275168888,-4.305846944)),
(E'Djibouti', 'http://www.wikidata.org/entity/Q977',E'Djibouti', ST_MakePoint(43.16331,11.56439)),
(E'Dominican Republic', 'http://www.wikidata.org/entity/Q786',E'Santo Domingo', ST_MakePoint(-69.9358436,18.4743214)),
(E'Ecuador', 'http://www.wikidata.org/entity/Q736',E'Quito', ST_MakePoint(-78.47886,-0.17767)),
(E'Egypt', 'http://www.wikidata.org/entity/Q79',E'Cairo Governorate', ST_MakePoint(31.2189901,30.0550073)),
(E'El Salvador', 'http://www.wikidata.org/entity/Q792',E'San Salvador', ST_MakePoint(-89.2326487,13.707291)),
(E'Equatorial Guinea', 'http://www.wikidata.org/entity/Q983',E'Malabo', ST_MakePoint(8.74264,3.75413)),
(E'Eritrea', 'http://www.wikidata.org/entity/Q986',E'Asmara', ST_MakePoint(38.9217022,15.3249125)),
(E'Estonia', 'http://www.wikidata.org/entity/Q191',E'Tallinn City', ST_MakePoint(24.7374,59.4289)),
(E'Ethiopia', 'http://www.wikidata.org/entity/Q115',E'Addis Ababa', ST_MakePoint(38.7795,9.04008)),
(E'Finland', 'http://www.wikidata.org/entity/Q33',E'Helsinki', ST_MakePoint(24.867066666,60.185905555)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'2nd arrondissement of Marseille', ST_MakePoint(5.368,43.305028)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'6th arrondissement of Lyon', ST_MakePoint(4.84948,45.7749)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'Bordeaux', ST_MakePoint(-0.594615,44.8526)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'Brest', ST_MakePoint(-4.48709,48.3893)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'Paris', ST_MakePoint(2.30968,48.8661)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'Rouen', ST_MakePoint(1.08391,49.4288)),
(E'France', 'http://www.wikidata.org/entity/Q142',E'Strasbourg', ST_MakePoint(7.7621,48.5875)),
(E'Gabon', 'http://www.wikidata.org/entity/Q1000',E'Libreville', ST_MakePoint(9.44547,0.38424)),
(E'Georgia', 'http://www.wikidata.org/entity/Q230',E'Old Tbilisi', ST_MakePoint(44.8230214,41.6891044)),
(E'Ghana', 'http://www.wikidata.org/entity/Q117',E'Greater Accra Region', ST_MakePoint(-0.198675,5.5673662)),
(E'Greece', 'http://www.wikidata.org/entity/Q41',E'Athens', ST_MakePoint(23.744033,37.976103)),
(E'Guatemala', 'http://www.wikidata.org/entity/Q774',E'Guatemala City', ST_MakePoint(-90.5150126,14.6045423)),
(E'Guinea', 'http://www.wikidata.org/entity/Q1006',E'Kaloum', ST_MakePoint(-13.7160043,9.5122736)),
(E'Haiti', 'http://www.wikidata.org/entity/Q790',E'Pétion-Ville', ST_MakePoint(-72.2806,18.5068)),
(E'Honduras', 'http://www.wikidata.org/entity/Q783',E'Francisco Morazán Department', ST_MakePoint(-87.1819521,14.0948563)),
(E'Hungary', 'http://www.wikidata.org/entity/Q28',E'Budapest', ST_MakePoint(19.029,47.503)),
(E'Iceland', 'http://www.wikidata.org/entity/Q189',E'Reykjavík', ST_MakePoint(-21.9366085,64.1427805)),
(E'India', 'http://www.wikidata.org/entity/Q668',E'Bangalore', ST_MakePoint(77.6004,12.9683)),
(E'India', 'http://www.wikidata.org/entity/Q668',E'New Delhi', ST_MakePoint(77.1883,28.58915)),
(E'Indonesia', 'http://www.wikidata.org/entity/Q252',E'Jakarta', ST_MakePoint(106.82368,-6.19681)),
(E'Iran', 'http://www.wikidata.org/entity/Q794',E'Tehran', ST_MakePoint(51.42045,35.6931)),
(E'Iraq', 'http://www.wikidata.org/entity/Q796',E'Baghdad Governorate', ST_MakePoint(44.3447201,33.3151544)),
(E'Israel', 'http://www.wikidata.org/entity/Q801',E'Tel Aviv', ST_MakePoint(34.788611111,32.061666666)),
(E'Italy', 'http://www.wikidata.org/entity/Q38',E'', ST_MakePoint(12.321066661,45.437650663)),
(E'Italy', 'http://www.wikidata.org/entity/Q38',E'Rome', ST_MakePoint(12.485141,41.920502)),
(E'Italy', 'http://www.wikidata.org/entity/Q38',E'Rome', ST_MakePoint(12.503275,41.905117)),
(E'Ivory Coast', 'http://www.wikidata.org/entity/Q1008',E'Abidjan', ST_MakePoint(-4.005697222,5.328477777)),
(E'Jamaica', 'http://www.wikidata.org/entity/Q766',E'Kingston Parish', ST_MakePoint(-76.79143,18.01736)),
(E'Japan', 'http://www.wikidata.org/entity/Q17',E'Minato-ku', ST_MakePoint(139.725222,35.650806)),
(E'Japan', 'http://www.wikidata.org/entity/Q17',E'Osaka Prefecture', ST_MakePoint(135.49013889,34.70527778)),
(E'Jordan', 'http://www.wikidata.org/entity/Q810',E'Amman', ST_MakePoint(35.890211,31.955367)),
(E'Kazakhstan', 'http://www.wikidata.org/entity/Q232',E'Esil District', ST_MakePoint(71.4243747,51.1474719)),
(E'Kenya', 'http://www.wikidata.org/entity/Q114',E'Nairobi', ST_MakePoint(36.78503,-1.26859)),
(E'Kingdom of Denmark', 'http://www.wikidata.org/entity/Q756617',E'Copenhagen Municipality', ST_MakePoint(12.59737,55.706925)),
(E'Kosovo', 'http://www.wikidata.org/entity/Q1246',E'Prishtina Municipality', ST_MakePoint(21.1534852,42.6689621)),
(E'Kuwait', 'http://www.wikidata.org/entity/Q817',E'Kuwait City', ST_MakePoint(47.99311,29.37934)),
(E'Kyrgyzstan', 'http://www.wikidata.org/entity/Q813',E'Bishkek', ST_MakePoint(74.603556,42.868782)),
(E'Laos', 'http://www.wikidata.org/entity/Q819',E'Sisattanak District', ST_MakePoint(102.625562,17.94421)),
(E'Latvia', 'http://www.wikidata.org/entity/Q211',E'Riga', ST_MakePoint(24.114684,56.951578)),
(E'Lebanon', 'http://www.wikidata.org/entity/Q822',E'Matn District', ST_MakePoint(35.544709,33.8825421)),
(E'Liberia', 'http://www.wikidata.org/entity/Q1014',E'Greater Monrovia District', ST_MakePoint(-10.74558,6.27301)),
(E'Lithuania', 'http://www.wikidata.org/entity/Q37',E'Vilnius', ST_MakePoint(25.26443,54.68383)),
(E'Luxembourg', 'http://www.wikidata.org/entity/Q32',E'Luxembourg', ST_MakePoint(6.1214088,49.6129319)),
(E'Madagascar', 'http://www.wikidata.org/entity/Q1019',E'Antananarivo-Renivohitra District', ST_MakePoint(47.536194,-18.908781)),
(E'Malawi', 'http://www.wikidata.org/entity/Q1020',E'Lilongwe District', ST_MakePoint(33.7881201,-13.9600071)),
(E'Malaysia', 'http://www.wikidata.org/entity/Q833',E'Bukit Bintang', ST_MakePoint(101.7208143,3.1565312)),
(E'Mali', 'http://www.wikidata.org/entity/Q912',E'Commune 5', ST_MakePoint(-7.9882471,12.6249771)),
(E'Malta', 'http://www.wikidata.org/entity/Q233',E'Ta\' Xbiex', ST_MakePoint(14.4967014,35.898174)),
(E'Mauritania', 'http://www.wikidata.org/entity/Q1025',E'Nouakchott', ST_MakePoint(-15.9745,18.096)),
(E'Mexico', 'http://www.wikidata.org/entity/Q96',E'Mexico City', ST_MakePoint(-99.2053,19.4347)),
(E'Moldova', 'http://www.wikidata.org/entity/Q217',E'Chișinău', ST_MakePoint(28.81666667,47)),
(E'Mongolia', 'http://www.wikidata.org/entity/Q711',E'Ulaanbaatar', ST_MakePoint(106.9171634,47.9245002)),
(E'Montenegro', 'http://www.wikidata.org/entity/Q236',E'Podgorica Municipality', ST_MakePoint(19.261574,42.4430085)),
(E'Morocco', 'http://www.wikidata.org/entity/Q1028',E'Rabat', ST_MakePoint(-6.8330786,34.018196)),
(E'Mozambique', 'http://www.wikidata.org/entity/Q1029',E'KaMpfumo district', ST_MakePoint(32.598611,-25.958889)),
(E'Myanmar', 'http://www.wikidata.org/entity/Q836',E'Bahan Township', ST_MakePoint(96.16253,16.80246)),
(E'Namibia', 'http://www.wikidata.org/entity/Q1030',E'Khomas Region', ST_MakePoint(17.083889,-22.567778)),
(E'Nepal', 'http://www.wikidata.org/entity/Q837',E'Kathmandu', ST_MakePoint(85.3334,27.70952)),
(E'Netherlands', 'http://www.wikidata.org/entity/Q55',E'The Hague', ST_MakePoint(4.2891,52.0848)),
(E'New Zealand', 'http://www.wikidata.org/entity/Q664',E'Wellington City', ST_MakePoint(174.780089,-41.270989)),
(E'Nicaragua', 'http://www.wikidata.org/entity/Q811',E'District I', ST_MakePoint(-86.25815,12.1154)),
(E'Niger', 'http://www.wikidata.org/entity/Q1032',E'Niamey', ST_MakePoint(2.09963,13.51941)),
(E'Nigeria', 'http://www.wikidata.org/entity/Q1033',E'Abuja Municipal', ST_MakePoint(7.48206,9.09791)),
(E'North Korea', 'http://www.wikidata.org/entity/Q423',E'Taedonggang-guyok', ST_MakePoint(125.792127,39.022957)),
(E'North Macedonia', 'http://www.wikidata.org/entity/Q221',E'Karpoš Municipality', ST_MakePoint(21.3963,41.99347)),
(E'Norway', 'http://www.wikidata.org/entity/Q20',E'Oslo municipality', ST_MakePoint(10.722616666,59.920116666)),
(E'Oman', 'http://www.wikidata.org/entity/Q842',E'Muscat Governorate', ST_MakePoint(58.5352907,23.5871671)),
(E'Pakistan', 'http://www.wikidata.org/entity/Q843',E'Islamabad', ST_MakePoint(73.1041,33.724)),
(E'Panama', 'http://www.wikidata.org/entity/Q804',E'Panama City', ST_MakePoint(-79.52015,8.9804764)),
(E'Paraguay', 'http://www.wikidata.org/entity/Q733',E'Asunción', ST_MakePoint(-57.56267,-25.28452)),
(E'People\s Republic of China', 'http://www.wikidata.org/entity/Q148',E'Beijing', ST_MakePoint(116.45166667,39.94166667)),
(E'People\'s Republic of China', 'http://www.wikidata.org/entity/Q148',E'Hong Kong', ST_MakePoint(114.165416,22.278648)),
(E'People\'s Republic of China', 'http://www.wikidata.org/entity/Q148',E'Shanghai', ST_MakePoint(121.442916,31.209114)),
(E'Peru', 'http://www.wikidata.org/entity/Q419',E'San Isidro', ST_MakePoint(-77.0219724,-12.0959214)),
(E'Philippines', 'http://www.wikidata.org/entity/Q928',E'Makati', ST_MakePoint(121.0164584,14.5608224)),
(E'Poland', 'http://www.wikidata.org/entity/Q36',E'Wrzeszcz Górny', ST_MakePoint(18.6223,54.3741)),
(E'Poland', 'http://www.wikidata.org/entity/Q36',E'Śródmieście', ST_MakePoint(21.02957,52.2231)),
(E'Portugal', 'http://www.wikidata.org/entity/Q45',E'Lisbon', ST_MakePoint(-9.140787,38.721273)),
(E'Qatar', 'http://www.wikidata.org/entity/Q846',E'Doha', ST_MakePoint(51.495144,25.310738)),
(E'Republic of Ireland', 'http://www.wikidata.org/entity/Q27',E'Dublin', ST_MakePoint(-6.203125,53.31095)),
(E'Republic of the Congo', 'http://www.wikidata.org/entity/Q971',E'Brazzaville', ST_MakePoint(15.282139,-4.274752)),
(E'Romania', 'http://www.wikidata.org/entity/Q218',E'Bucharest', ST_MakePoint(26.08725,44.45945)),
(E'Russia', 'http://www.wikidata.org/entity/Q159',E'Moscow', ST_MakePoint(37.517,55.71736)),
(E'Rwanda', 'http://www.wikidata.org/entity/Q1037',E'Kigali', ST_MakePoint(30.0641266,-1.9469921)),
(E'Saudi Arabia', 'http://www.wikidata.org/entity/Q851',E'Riyadh', ST_MakePoint(46.6238156,24.6861534)),
(E'Senegal', 'http://www.wikidata.org/entity/Q1041',E'Dakar', ST_MakePoint(-17.436778,14.661)),
(E'Serbia', 'http://www.wikidata.org/entity/Q403',E'Savski Venac', ST_MakePoint(20.4593,44.7772)),
(E'Sierra Leone', 'http://www.wikidata.org/entity/Q1044',E'Western Area', ST_MakePoint(-13.2528211,8.4618975)),
(E'Singapore', 'http://www.wikidata.org/entity/Q334',E'Singapore', ST_MakePoint(103.8519705,1.2847742)),
(E'Slovakia', 'http://www.wikidata.org/entity/Q214',E'Bratislava I', ST_MakePoint(17.1066142,48.1407862)),
(E'Slovenia', 'http://www.wikidata.org/entity/Q215',E'Ljubljana', ST_MakePoint(14.4985,46.052)),
(E'South Africa', 'http://www.wikidata.org/entity/Q258',E'Pretoria', ST_MakePoint(28.2226,-25.77056)),
(E'South Korea', 'http://www.wikidata.org/entity/Q884',E'Jung District', ST_MakePoint(126.9736064,37.5558016)),
(E'South Sudan', 'http://www.wikidata.org/entity/Q958',E'Juba', ST_MakePoint(31.5861063,4.8547463)),
(E'Spain', 'http://www.wikidata.org/entity/Q29',E'Madrid', ST_MakePoint(-3.690708333,40.429825)),
(E'Sri Lanka', 'http://www.wikidata.org/entity/Q854',E'Colombo', ST_MakePoint(79.85721,6.89836)),
(E'Sudan', 'http://www.wikidata.org/entity/Q1049',E'Khartoum', ST_MakePoint(32.5378054,15.6052343)),
(E'Sweden', 'http://www.wikidata.org/entity/Q34',E'Stockholm Municipality', ST_MakePoint(18.1063,59.3344)),
(E'Switzerland', 'http://www.wikidata.org/entity/Q39',E'Bern', ST_MakePoint(7.46214,46.93528)),
(E'Syria', 'http://www.wikidata.org/entity/Q858',E'Damascus', ST_MakePoint(36.2745611,33.5184227)),
(E'Tajikistan', 'http://www.wikidata.org/entity/Q863',E'Dushanbe', ST_MakePoint(68.748,38.5808)),
(E'Tanzania', 'http://www.wikidata.org/entity/Q924',E'Dar es Salaam', ST_MakePoint(39.2915788,-6.8126253)),
(E'Thailand', 'http://www.wikidata.org/entity/Q869',E'Bangkok', ST_MakePoint(100.541933,13.724064)),
(E'Togo', 'http://www.wikidata.org/entity/Q945',E'Lomé', ST_MakePoint(1.209,6.1173)),
(E'Trinidad and Tobago', 'http://www.wikidata.org/entity/Q754',E'Port of Spain', ST_MakePoint(-61.5225963,10.6678731)),
(E'Tunisia', 'http://www.wikidata.org/entity/Q948',E'Tunis', ST_MakePoint(10.242901,36.83305)),
(E'Turkey', 'http://www.wikidata.org/entity/Q43',E'Ankara', ST_MakePoint(32.855967,39.904609)),
(E'Turkey', 'http://www.wikidata.org/entity/Q43',E'Beşiktaş', ST_MakePoint(28.98916667,41.03527778)),
(E'Turkmenistan', 'http://www.wikidata.org/entity/Q874',E'Ashgabat', ST_MakePoint(58.3644571,37.9524949)),
(E'Uganda', 'http://www.wikidata.org/entity/Q1036',E'Kampala District', ST_MakePoint(32.588611111,0.330555555)),
(E'Ukraine', 'http://www.wikidata.org/entity/Q212',E'Kyiv', ST_MakePoint(30.511111,50.446111)),
(E'United Arab Emirates', 'http://www.wikidata.org/entity/Q878',E'Emirate of Abu Dhabi', ST_MakePoint(54.38278,24.4958547)),
(E'United Kingdom', 'http://www.wikidata.org/entity/Q145',E'Belgravia', ST_MakePoint(-0.15425,51.49825)),
(E'Uruguay', 'http://www.wikidata.org/entity/Q77',E'Montevideo Department', ST_MakePoint(-56.184361,-34.914222)),
(E'Uzbekistan', 'http://www.wikidata.org/entity/Q265',E'Tashkent', ST_MakePoint(69.272222222,41.325555555)),
(E'Venezuela', 'http://www.wikidata.org/entity/Q717',E'Caracas', ST_MakePoint(-66.8519702,10.498791)),
(E'Vietnam', 'http://www.wikidata.org/entity/Q881',E'Hanoi', ST_MakePoint(105.8406964,21.0305184)),
(E'Zambia', 'http://www.wikidata.org/entity/Q953',E'Lusaka', ST_MakePoint(28.30894,-15.4195)),
(E'Zimbabwe', 'http://www.wikidata.org/entity/Q954',E'Harare Province', ST_MakePoint(31.0378,-17.7866));


/*
 * Resolving the coordinates stored in 'geom' and to retrieve the full address of each 
 * embassy using nominatim_reverse()
 */
DO $$
DECLARE 
 rec german_embassy%rowtype;
 addr text;
BEGIN
  FOR rec IN
    SELECT * FROM public.german_embassy
  LOOP
   RAISE NOTICE 'Resolving coordinates "%" (%) ...',ST_AsLatLonText(rec.geom), rec.country;
   SELECT result INTO addr 
   FROM nominatim_reverse(
          server_name => 'osm', 
					lon => ST_X(rec.geom), 
					lat => ST_Y(rec.geom));
   IF addr IS NOT NULL THEN
     UPDATE german_embassy 
	 SET address = addr 
	 WHERE id = rec.id;
	 EXECUTE pg_sleep(2); -- waits 2 seconds between requests to avoid any trouble with OSM.
   END IF;    
  END LOOP;
END; $$;

/*
 * This uses the function nominatim_search() to retrieve coordiantes from the addresses
 * we retrieved using nomimatim_reverse(). Since we already have the original coordinates 
 * in the table, we can check if the coordinates retrieved match the orginal ones and 
 * caculate their distance in case they differ. The distances are caculated in metres and 
 * are stored in the column 'distance'.
 */
DO $$
DECLARE 
 rec german_embassy%rowtype;
 g geometry(point,4326);
BEGIN
  FOR rec IN
    SELECT * FROM public.german_embassy
  LOOP
   RAISE NOTICE 'Resolving addresses "%" ...', rec.address;
   SELECT ST_MakePoint(lon,lat) INTO g
   FROM nominatim_search(
          server_name => 'osm', 
		  q => rec.address);
   UPDATE german_embassy 
   SET distance = ST_Distance(g::geography,rec.geom::geography) 
   WHERE id = rec.id;   
   EXECUTE pg_sleep(2); -- waits 2 seconds between requests to avoid any trouble with OSM.
  END LOOP;
END; $$;

SELECT * FROM german_embassy;