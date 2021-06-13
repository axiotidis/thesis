var userOrp = "";
var userPh = "";
var userTemp = "";
var userTrb = "";
var lastUpdate = "";
var orpMax = "";
var orpMin = "";
var phMax = "";
var phMin = "";
var tempMax = "";
var tempMin = "";
var trbMax = "";
var trbMin = "";
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
	if (user) {
		user = firebase.auth().currentUser;
		var userDisplayName = user.displayName;
		//read user details
		getUserId(userDisplayName);
		
		
		
		} else {
					location.replace("index.html");
				}
					
	});

function getUserId(currentUser){
	var ref = firebase.database().ref("users");
	ref.once("value")
		.then(function(snapshot){
			var userId = snapshot.child(currentUser + "/id").val();
			console.log("userId= "+ userId);
			if (userId != null) {
				readTotal(userId);
				readSensors(userId);
				readReference();
			}
			else {
				alert("Δεν έχετε κάνει εγγραφή στην εφαρμογή κάντε κλικ στο πλήκτρο \"OK\" για να μεταφερθείτε στην οθόνη των ρυθμίσεων");
				location.replace("mysettings.html");
			}
		});
}




function readTotal(userId){
	var ref = firebase.database().ref("customers");
	ref.once("value")
		.then(function(snapshot){
			var userTotal = snapshot.child(userId + "/consumption/total").val();
			console.log("userTotal= "+ userTotal);
			var txt = "3";
			if (userTotal == null){
				userTotal = "0.0";
			}
			document.getElementById("consumtion").innerHTML = "<b>" + userTotal + " m" + txt.sup() + "<b>"; //update the total consumption value
		});
}

function readSensors(userId){
	var ref = firebase.database().ref("customers/" + userId + "/sensors");
	ref.once("value")
		.then(function(snapshot){
		ref.on("value" , gotData , errData);
		
	});
}

function readReference(){
	var ref = firebase.database().ref("reference");
	ref.once("value")
		.then(function(snapshot){
		ref.on("value" , gotRefData , errData);
	});
}

function gotData(data){
	data = data.val();
	userOrp = data.orp;
	//console.log("userOrp= " + userOrp);
	userPh = data.ph;
	//console.log("userPh= " + userPh);
	userTemp = data.temp;
	//console.log("userTemp= " + userTemp);
	userTrb = data.trb;
	//console.log("userTrb= " + userTrb);
	lastUpdate = data.lastupdate;
	console.log("lastUpdate= " + lastUpdate);
	document.getElementById("last_update").innerHTML = lastUpdate; //update the last update value
}

function gotRefData(data){
	data = data.val();
	orpMax = data.orpmax;
	console.log("orpMax= " + orpMax);
	orpMin = data.orpmin;
	console.log("orpMin= " + orpMin);
	console.log("userOrp= " + userOrp);
	phMax = data.phmax;
	console.log("phMax= " + phMax);
	phMin = data.phmin;
	console.log("phMin= " + phMin);
	console.log("userPh= " + userPh);
	tempMax = data.tempmax;
	console.log("tempMax= " + tempMax);
	tempMin = data.tempmin;
	console.log("tempMin= " + tempMin);
	console.log("userTemp= " + userTemp);
	trbMax = data.trbmax;
	console.log("trbMax= " + trbMax);
	trbMin = data.trbmin;
	console.log("trbMin= " + trbMin);
	console.log("userTrb= " + userTrb);
	if (parseFloat(userOrp) >  parseFloat(orpMax) || parseFloat(userOrp) <  parseFloat(orpMin)){
		orpIndex = true;
		console.log("Be careful ORP is out of range");
	}
	else {
		orpIndex = false;
	}
	
	if (parseFloat(userPh) >  parseFloat(phMax) || parseFloat(userPh) <  parseFloat(phMin)){
		phIndex = true;
		console.log("Be careful ph is out of range");
	}
	else {
		phIndex = false;
	}
	
	if (parseFloat(userTemp) >  parseFloat(tempMax) || parseFloat(userTemp) <  parseFloat(tempMin)){
		tempIndex = true;
		console.log("Be careful Temperature is out of range");
	}
	else {
		tempIndex = false;
	}
	
	if (parseFloat(userTrb) >  parseFloat(trbMax) || parseFloat(userTrb) <  parseFloat(trbMin)){
		trbIndex = true;
		console.log("Be careful Turbidity is out of range");
	}
	else {
		trbIndex = false;
	}
	
	if (orpIndex || phIndex || tempIndex || trbIndex){
		document.getElementById("safe_txt").style.backgroundColor = "#dc3545";
		document.getElementById("safe_txt").style.color = "#fff";
		//document.getElementById("safe_box").style.color = "#dc3545";
		//document.getElementById("safe_box").style.backgroundColor = "red"; //don't work
		//document.getElementById("safe_box").class="p-3 mb-2 bg-danger text-white" //don't work
		document.getElementById("safe_txt").innerHTML = "<br>Προσοχή!<br>Η ποιότητα του νερού που φθάνει στην παροχή σας δεν είναι κατάλληλη σύμφωνα με τις προδιαργαφές<br><br>";
		document.getElementById("safe_pic").src="pics/non_safe_water_small.png";
	}
	else {
		//document.getElementById("safe_box").class="p-3 mb-2 bg-info text-white"
		document.getElementById("safe_txt").style.backgroundColor = "#17a2b8";
		document.getElementById("safe_txt").style.color = "#fff";
		document.getElementById("safe_txt").innerHTML = "<br>Η ποιότητα του νερού που φθάνει στην παροχή σας είναι κατάλληλη και σύμφωνη με τις προδιαργαφές<br><br>";
		document.getElementById("safe_pic").src="pics/safe_water_small.png";
	}
	
}

function errData(error){
	console.log(error.message , error.code);
}
