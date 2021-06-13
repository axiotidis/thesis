var userAddress = "";
var userEmail = "";
var userFname = "";
var userLname = "";
var userId = "";
var userPhone = "";
var userDisplayName = "";
var userRole = "";
var username = "";

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

//Listen for form submit
document.getElementById("profileForm").addEventListener("submit", submitForm);



//Submit form
function submitForm(e){
    	e.preventDefault();
	
    	//Get values
    	userLname = getInputVal("inputLastname");
    	userFname = getInputVal("inputFirstname");
    	userAddress = getInputVal("inputAddress");
    	userPhone = getInputVal("inputMobile");
    	userEmail = getInputVal("inputEmail");
    	userId = getInputVal("inputMyid");
		username = getInputVal("inputUsername");
	
	
	updateDetails(userLname, userFname, userAddress, userPhone, userEmail, userId, userRole);
	
	
	
}

//Function to get form values
function getInputVal(id){
    	return document.getElementById(id).value;
}

//Save the details to firebase
function updateDetails(lastname, firstname, address, phone, email, id, role){
	var ref;
	if (role.localeCompare("") == 0){
		alert("Η αποθήκευση απέτυχε, δεν επιλέξατε το ρόλο του χρήστη");
	}
	else{
		if (role.localeCompare("user") == 0){
			ref = firebase.database().ref("customers");
			database.ref("customers/" + id + "/user").update({ 
			lastname: lastname,
			firstname: firstname,
			address: address,
			phone : phone,
			email : email
			});
			database.ref("customers/" + id + "/consumption/").update({
				total: "0.000"
			});	
			database.ref("customers/" + id + "/location/").update({
				lati: "41.11954131584224",
				logni: "25.399789810180664"
			});	
		}
		else {
			ref = firebase.database().ref("service");
			database.ref("service/" + username ).update({ 
			lastname: lastname,
			firstname: firstname,
			address: address,
			phone : phone,
			email : email,
			role : role
			});
		}
		alert("Τα στοιχεία του χρήστη αποθηκεύτηκαν επιτυχώς");
		document.getElementById('profileForm').reset();
	}
	
}

function roleFunction(role){
	userRole = role;
}

function logoutFunction(){
			firebase.auth().signOut().then(function()  {
			location.replace("index.html");
			}).catch(function(error) {
				alert(error);
			});
}

function snapshotLabelToArray(snapshot) {
	var returnArray = [];
	
	snapshot.forEach(function(childSnapshot) {
		var label = childSnapshot.key;
		returnArray.push(label);
	});
	
	return returnArray;
};
