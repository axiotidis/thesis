var basemap = new L.TileLayer(baseUrl, {maxZoom: 19, attribution: baseAttribution, subdomains: subdomains, opacity: opacity});

var klat = 41.11954131584224;		//set initial value latitude
var klng = 25.399789810180664;		//set initial value lognitude

var zoom = 15;		//set zoom level

var refOrpLow;
var refOrpHigh;
var refPhLow;
var refPhHigh;
var refTempLow;
var refTempHigh;
var refTrbLow;
var refTrbHigh;
var customers = [];
var servicePersnl = [];
var orpIndex = false;
var phIndex = false;
var tempIndex = false;
var trbIndex = false;



//web app's Firebase configuration
//replace the xxxxxxxx with your firebase project details
var firebaseConfig = {
	apiKey: "xxxxxxxxxxxxxxxxxxxxxxx",
	authDomain: "xxxxxxxxxxxxx.firebaseapp.com",
	databaseURL: "https://xxxxxxxxxxxxx.firebaseio.com",
	projectId: "xxxxxxxxxxxx",
	storageBucket: "xxxxxxxxx.appspot.com",
	messagingSenderId: "xxxxxxxxxxx",
	appId: "xxxxxxxxxxxxxxxxxx",
	measurementId: "xxxxxxxxxxxxx"
	};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
// Get a reference to the database service
let database = firebase.database();

firebase.auth().onAuthStateChanged(function(user) {
	console.log("function authFunction");
	if (user) {
		user = firebase.auth().currentUser;
		userDisplayName = user.displayName;
		
		var rootRef = firebase.database().ref();
		var urlRef = rootRef.child("service");

		urlRef.once("value", function(snapshot) {
			servicePersnl = snapshotLabelToArray(snapshot);
			console.log("servicePersnl = " + servicePersnl);
			
			ref = firebase.database().ref();
			ref.once("value")
			.then(function(snapshot){
			var path = "service/" + userDisplayName + "/role";
			console.log("path = " + path);
			var role = snapshot.child(path).val();
			console.log("role = " + role);
			if (!servicePersnl.includes(userDisplayName)){
				alert("Δεν έχετε δικαιώματα διαχειριστή");
				logoutFunction();
			}
			else{
				if (role.localeCompare("admin") != 0){
					alert("Δεν έχετε δικαιώματα διαχειριστή");
					logoutFunction();
				}
			}
			});
		});
		} 
		else {
				location.replace("index.html");
			}
					
	});
var bluePin = L.icon({			//set a marker icon for current location of user
	iconUrl: 'pics/blue_pin.png',
	//shadowUrl: 'img/leaf-shadow.png',

	iconSize:     [38, 46], // size of the icon
	//shadowSize:   [50, 64], // size of the shadow
	iconAnchor:   [22, 94], // point of the icon which will correspond to marker's location
	//shadowAnchor: [4, 62],  // the same for the shadow
	//popupAnchor:  [-3, -76] // point from which the popup should open relative to the iconAnchor
	popupAnchor:  [-6, -86] // point from which the popup should open relative to the iconAnchor
});	

var alertPin = L.icon({			//set a marker icon for booked scooters
	iconUrl: 'pics/alert_pin.png',
	//shadowUrl: 'img/leaf-shadow.png',

	iconSize:     [34, 41], // size of the icon
	//shadowSize:   [50, 64], // size of the shadow
	iconAnchor:   [22, 94], // point of the icon which will correspond to marker's location
	//shadowAnchor: [4, 62],  // the same for the shadow
	//popupAnchor:  [-3, -76] // point from which the popup should open relative to the iconAnchor
	popupAnchor:  [-6, -86] // point from which the popup should open relative to the iconAnchor
});

var safePin = L.icon({			//set a marker icon for free scooters
	iconUrl: 'pics/safe_pin.png',
	//shadowUrl: 'img/leaf-shadow.png',

	iconSize:     [34, 41], // size of the icon
	//shadowSize:   [50, 64], // size of the shadow
	iconAnchor:   [22, 94], // point of the icon which will correspond to marker's location
	//shadowAnchor: [4, 62],  // the same for the shadow
	//popupAnchor:  [-3, -76] // point from which the popup should open relative to the iconAnchor
	popupAnchor:  [-6, -86] // point from which the popup should open relative to the iconAnchor
});

//center and zoom map in a position found by geolocation
var center = new L.LatLng(klat, klng);
//var map = new L.map('map', {center: center, zoomControl: false, maxZoom: maxZoom, layers: [basemap] });
var map = new L.map('map', {center: center, zoomControl: false, minZoom: 0, maxZoom: 50, layers: [basemap] });
var popup = L.popup();

// Disable some features in desktop application
if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition(setPosition);
  }


function setPosition(position) {
  //lat = position.coords.latitude.toString();		//find latitude
  //lng = position.coords.longitude.toString();		//find lognitude
  //var marker = new L.marker([lat, lng], {icon: bluePin}).addTo(map);	//set a marker in current geoposition
  //var mypopup = "βρισκεστε εδω";
  map.setView([klat, klng], zoom);			//No auto pan!!!!!!!!!!!!!!!!
  //map.setView([lat, lng], zoom);			//No auto pan!!!!!!!!!!!!!!!!
  //marker.bindPopup(mypopup);
}

function onLocationFound(e) {
        //do nothing
	
    }

    function onLocationError(e) {
        alert(e.message);
    }



map.on('locationfound', onLocationFound);
map.on('locationerror', onLocationError);



let ref = database.ref("reference"); 
ref.on("value" , gotData , errdata);

function gotData(data){
	console.log("function = gotData");
	data = data.val();
	refOrpLow = data.orpmin;
	refOrpHigh = data.orpmax;
	refPhLow = data.phmin;
	refPhHigh = data.phmax;
	refTempLow = data.tempmin;
	refTempHigh = data.tempmax;
	refTrbLow = data.trbmin;
	refTrbHigh = data.trbmax;
}

function errdata(error){
	console.log(error.message , error.code);
}



var rootRef = firebase.database().ref();
var urlRef = rootRef.child("customers");


urlRef.once("value", function(snapshot) {
		
		customers = snapshotLabelToArray(snapshot);
		console.log("customers = " + customers);
		
		ref = firebase.database().ref();
		ref.once("value")
		.then(function(snapshot){
			var i;
			for (i=0; i<customers.length; ++i){
				var path = "customers/" + customers[i] + "/location/logni";
				console.log("path = " + path);
				var userLogn = snapshot.child(path).val();
				console.log(customers[i] + " - Lognitude = " + userLogn);
				path = "customers/" + customers[i] + "/location/lati";
				console.log("path = " + path);
				var userLat = snapshot.child(path).val();
				console.log(customers[i] + " - Latitude = " + userLat);
				path = "customers/" + customers[i] + "/sensors/orp";
				console.log("path = " + path);
				var userOrp = snapshot.child(path).val();
				console.log(customers[i] + " - ORP = " + userOrp);
				path = "customers/" + customers[i] + "/sensors/ph";	
				console.log("path = " + path);
				var userPh = snapshot.child(path).val();
				console.log(customers[i] + " - Ph = " + userPh);
				path = "customers/" + customers[i] + "/sensors/temp";	
				console.log("path = " + path);
				var userTemp = snapshot.child(path).val();
				console.log(customers[i] + " - Temperature = " + userTemp);
				path = "customers/" + customers[i] + "/sensors/trb";	
				console.log("path = " + path);
				var userTrb = snapshot.child(path).val();
				console.log(customers[i] + "Turbidity = " + userTrb);
				console.log("refOrpLow = " + refOrpLow);
				console.log("refOrpHigh = " + refOrpHigh);
				console.log("refPhLow = " + refPhLow);
				console.log("refPhHigh = " + refPhHigh);
				console.log("refTempLow = " + refTempLow);
				console.log("refTempHigh = " + refTempHigh);
				console.log("refTrbLow = " + refTrbLow);
				console.log("refTrbHigh = " + refTrbHigh);
				var alertpopup = "Αριθ. Παροχής: " + customers[i] + "<br>";
				if (parseFloat(userOrp) >  parseFloat(refOrpHigh) || parseFloat(userOrp) <  parseFloat(refOrpLow)){
					orpIndex = true;
					console.log("Be careful ORP is out of range");
					alertpopup += "Ο δείκτης ORP είναι εκτός ορίων<br>";
					alertpopup += "ORP = ";
					alertpopup += userOrp;
					alertpopup += " mV<br>";
					}
				else {
					orpIndex = false;
					}
	
				if (parseFloat(userPh) >  parseFloat(refPhHigh) || parseFloat(userPh) <  parseFloat(refPhLow)){
					phIndex = true;
					console.log("Be careful ph is out of range");
					alertpopup += "Ο δείκτης ph είναι εκτός ορίων<br>";
					alertpopup += "ph = ";
					alertpopup += userPh;
					}
				else {
					phIndex = false;
					}
	
				if (parseFloat(userTemp) >  parseFloat(refTempHigh) || parseFloat(userTemp) <  parseFloat(refTempLow)){
					tempIndex = true;
					console.log("Be careful Temperature is out of range");
					alertpopup += "Η θερμοκρασία του νερού είναι εκτός ορίων<br>";
					alertpopup += "θερμοκρασία = ";
					alertpopup += userTemp;
					alertpopup += " C<br>";
					}
				else {
					tempIndex = false;
					}
	
				if (parseFloat(userTrb) >  parseFloat(refTrbHigh) || parseFloat(userTrb) <  parseFloat(refTrbLow)){
					trbIndex = true;
					console.log("Be careful Turbidity is out of range");
					alertpopup += "Η θολότητα του νερού είναι εκτός ορίων<br>";
					alertpopup += "θολότητα = ";
					alertpopup += userTrb;
					alertpopup += " NTU<br>";
					}
				else {
					trbIndex = false;
					}
				if (orpIndex || phIndex || tempIndex || trbIndex){
					//put an alert pin
					var marker = new L.marker([userLat, userLogn], {icon: alertPin}).addTo(map);	//set a marker in current geoposition
					//marker.bindPopup(alertpopup);
					marker.bindPopup(alertpopup);
				}
				else {
					//put a sefe pin
					var marker = new L.marker([userLat, userLogn], {icon: safePin}).addTo(map);	//set a marker in current geoposition
					var safepopup = "Αριθ. Παροχής: " + customers[i] + "<br>Το νερό είναι ασφαλές για κατανάλωση";		//prepare a custom popup 
					//marker.bindPopup(safepopup);
					marker.bindPopup(safepopup);
				}
				
				}
			});
			
		
		
		
		
		
	});

function snapshotLabelToArray(snapshot) {
	var returnArray = [];
	
	snapshot.forEach(function(childSnapshot) {
		var label = childSnapshot.key;
		returnArray.push(label);
	});
	
	return returnArray;
};

function logoutFunction(){
			firebase.auth().signOut().then(function()  {
			location.replace("index.html");
			}).catch(function(error) {
				alert(error);
			});
			}

