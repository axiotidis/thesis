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
