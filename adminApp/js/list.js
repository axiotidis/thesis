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
			
			if (!servicePersnl.includes(userDisplayName)){
				location.replace("index.html");
			}
		});
		} 
		else {
				location.replace("index.html");
			}
					
	});

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
			
			var userLname = "";
			var userFname = "";
			var userAddrss = "";
			var userPhone = "";
			var userOrp = "";
			var userPh = "";
			var userTemp = "";
			var userTrb = "";
			for (i=0; i<customers.length; ++i){
				var problem = "";
				var path = "customers/" + customers[i] + "/user/lastname";
				console.log("path = " + path);
				userLname = snapshot.child(path).val();
				console.log(customers[i] + " - User lastname = " + userLname);
				path = "customers/" + customers[i] + "/user/firstname";
				console.log("path = " + path);
				userFname = snapshot.child(path).val();
				console.log(customers[i] + " - User firstname = " + userFname);
				path = "customers/" + customers[i] + "/user/address";
				console.log("path = " + path);
				userAddrss = snapshot.child(path).val();
				console.log(customers[i] + " - User address = " + userAddrss);
				path = "customers/" + customers[i] + "/user/phone";
				console.log("path = " + path);
				userPhone = snapshot.child(path).val();
				console.log(customers[i] + " - User phone = " + userPhone);
				path = "customers/" + customers[i] + "/sensors/orp";
				console.log("path = " + path);
				userOrp = snapshot.child(path).val();
				console.log(customers[i] + " - ORP = " + userOrp);
				path = "customers/" + customers[i] + "/sensors/ph";	
				console.log("path = " + path);
				userPh = snapshot.child(path).val();
				console.log(customers[i] + " - Ph = " + userPh);
				path = "customers/" + customers[i] + "/sensors/temp";	
				console.log("path = " + path);
				userTemp = snapshot.child(path).val();
				console.log(customers[i] + " - Temperature = " + userTemp);
				path = "customers/" + customers[i] + "/sensors/trb";	
				console.log("path = " + path);
				userTrb = snapshot.child(path).val();
				console.log(customers[i] + "Turbidity = " + userTrb);
				console.log("refOrpLow = " + refOrpLow);
				console.log("refOrpHigh = " + refOrpHigh);
				console.log("refPhLow = " + refPhLow);
				console.log("refPhHigh = " + refPhHigh);
				console.log("refTempLow = " + refTempLow);
				console.log("refTempHigh = " + refTempHigh);
				console.log("refTrbLow = " + refTrbLow);
				console.log("refTrbHigh = " + refTrbHigh);
				
				if (parseFloat(userOrp) >  parseFloat(refOrpHigh) || parseFloat(userOrp) <  parseFloat(refOrpLow)){
					orpIndex = true;
					console.log("Be careful ORP is out of range");
					problem += "Ο δείκτης ORP είναι εκτός ορίων<br>";
					problem += "ORP = ";
					problem += userOrp;
					problem += " mV<br>";
					}
				else {
					orpIndex = false;
					}
	
				if (parseFloat(userPh) >  parseFloat(refPhHigh) || parseFloat(userPh) <  parseFloat(refPhLow)){
					phIndex = true;
					console.log("Be careful ph is out of range");
					problem += "Ο δείκτης ph είναι εκτός ορίων<br>";
					problem += "ph = ";
					problem += userPh;
					}
				else {
					phIndex = false;
					}
	
				if (parseFloat(userTemp) >  parseFloat(refTempHigh) || parseFloat(userTemp) <  parseFloat(refTempLow)){
					tempIndex = true;
					console.log("Be careful Temperature is out of range");
					problem += "Η θερμοκρασία του νερού είναι εκτός ορίων<br>";
					problem += "θερμοκρασία = ";
					problem += userTemp;
					problem += " C<br>";
					}
				else {
					tempIndex = false;
					}
	
				if (parseFloat(userTrb) >  parseFloat(refTrbHigh) || parseFloat(userTrb) <  parseFloat(refTrbLow)){
					trbIndex = true;
					console.log("Be careful Turbidity is out of range");
					problem += "Η θολότητα του νερού είναι εκτός ορίων<br>";
					problem += "θολότητα = ";
					problem += userTrb;
					problem += " NTU<br>";
					}
				else {
					trbIndex = false;
					}
				
				if (orpIndex || phIndex || tempIndex || trbIndex){
					var completelist= document.getElementById("thelist");
					completelist.innerHTML += "<li><b>" + userAddrss + "</b>, " + userLname + " " + userFname + "<br>" + "<b>Αριθ. Παροχής: </b>" + customers[i] + ",<br><b>Τηλ: </b>" + userPhone + "<br>" + problem + "<br></li>";
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


