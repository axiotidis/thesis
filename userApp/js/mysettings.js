var userAddress = "";
var userEmail = "";
var userFname = "";
var userLname = "";
var userId = "";
var userPhone = "";
var userDisplayName = "";

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
		userDisplayName = user.displayName;
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
				//user exists get user details
				readProfile(currentUser);
			}
			else {
				//user doesn't exist set user details
				alert("User doesn't exist");
			}
		});
}

function readProfile(currentUser){
	console.log("currentUser= " + currentUser);
	var ref = firebase.database().ref("users/" + currentUser);
	ref.once("value")
		.then(function(snapshot){
		ref.on("value" , gotData , errData);
		
	});
}

function gotData(data){
	data = data.val();
	userAddress = data.address;
	console.log("userAddress= " + userAddress);
	userEmail = data.email;
	console.log("userEmail= " + userEmail);
	userFname = data.firstname;
	console.log("userFname= " + userFname);
	userId = data.id;
	console.log("userId= " + userId);
	userLname = data.lastname;
	console.log("userLname= " + userLname);
	userPhone = data.phone;
	console.log("userPhone= " + userPhone);
	
	//fill the user form with stored user details
	document.getElementById("inputLastname").value = userLname; 
	document.getElementById("inputFirstname").value = userFname; 
	document.getElementById("inputAddress").value = userAddress;
	document.getElementById("inputMobile").value = userPhone;
	document.getElementById("inputEmail").value = userEmail;
	document.getElementById("inputMyid").value = userId;
}

function errData(error){
	console.log(error.message , error.code);
}

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
    
	// update user details
	
	updateDetails(userLname, userFname, userAddress, userPhone, userEmail, userId);
	alert("Τα στοιχεία σας ενημερώθηκαν επιτυχώς");

	
}

//Function to get form values
function getInputVal(id){
    	return document.getElementById(id).value;
}

//Save the details to firebase
function updateDetails(lastname, firstname, address, phone, email, id){
	var ref = firebase.database().ref("users");
    	database.ref("users/" + userDisplayName).update({ 
	    lastname: lastname,
	    firstname: firstname,
	    address: address,
	    phone : phone,
	    email : email,
	    id : id
	});

}
