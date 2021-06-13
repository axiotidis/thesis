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

//var dayHourArray = [];
var d = new Date();
var months = ["Ιανουάριος", "Φεβρουάριος", "Μάρτιος", "Απρίλιος", "Μάιος", "Ιούνιος", "Ιούλιος", "Αύγουστος", "Σεπτέμβριος", "Οκτώβριος", "Νοέμβριος", "Δεκέμβριος"];
var years = ["2020", "2021", "2022"];
var userId = "";
var selectedMonth = months[d.getMonth()];
var selectedYear = years[d.getYear()-120];
console.log("Current Month= "+ selectedMonth);
console.log("Current Year= "+ selectedYear);

document.getElementById("sMonth").innerHTML = selectedMonth;
document.getElementById("sYear").innerHTML = selectedYear;

function monthFunction(month){
	console.log("function monthFunction");
	selectedMonth = month;
	document.getElementById("sMonth").innerHTML = selectedMonth;
	//dayHourArray = [];
	readMonthlyConsumption(selectedYear, selectedMonth);
	}

function yearFunction(year){
	console.log("function yearFunction");
	selectedYear = year;
	document.getElementById("sYear").innerHTML = selectedYear;
	//dayHourArray = [];
	readMonthlyConsumption(selectedYear, selectedMonth);
	}

firebase.auth().onAuthStateChanged(function(user) {
	console.log("function authFunction");
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
	console.log("function getUserId");
	var ref = firebase.database().ref("users");
	ref.once("value")
		.then(function(snapshot){
			userId = snapshot.child(currentUser + "/id").val();
			console.log("userId= "+ userId);
			if (userId != null) {
				//user exists do something
				//dayHourArray = [];
				readMonthlyConsumption(selectedYear, selectedMonth);
				getLastUpdate(userId);
			}
			else {
				//user doesn't exist set user details
				alert("Δεν έχετε κάνει εγγραφή στην εφαρμογή κάντε κλικ στο πλήκτρο \"OK\" για να μεταφερθείτε στην οθόνη των ρυθμίσεων");
				location.replace("mysettings.html");
			}
		});
}

function readMonthlyConsumption(year, month){
	console.log("Read Monthly data for " + month + " " + year);
	

	
	var monthNmbr = "";
	sumOfMonth = 0;
	var txt = "3";
	document.getElementById("consumption").innerHTML = "<b>" + sumOfMonth + " m" + txt.sup() + "</b>";
	
	switch (month) {
		case "Ιανουάριος":
			monthNmbr = "1";
			break;
		case "Φεβρουάριος":
			monthNmbr = "2";
			break;	
		case "Μάρτιος":
			monthNmbr = "3";
			break;
		case "Απρίλιος":
			monthNmbr = "4";
			break;
		case "Μάιος":
			monthNmbr = "5";
			break;
		case "Ιούνιος":
			monthNmbr = "6";
			break;
		case "Ιούλιος":
			monthNmbr = "7";
			break;
		case "Αύγουστος":
			monthNmbr = "8";
			break;
		case "Σεπτέμβριος":
			monthNmbr = "9";
			break;
		case "Οκτώβριος":
			monthNmbr = "10";
			break;
		case "Νοέμβριος":
			monthNmbr = "11";
			break;
		case "Δεκέμβριος":
			monthNmbr = "12";
	}
	
	
	//var mRootRef = firebase.database().ref();
	var ref = firebase.database().ref();
	
	
	//var dayHourArray = [];
	
	var sumOfMonth = 0.000;
		
			
	ref.once("value")
	.then(function(snapshot){
		var day;
		var hour;
		var dayHourArray = [];
		var hourVolNmbr;
		var sumOfDay = [0.000];
		
		for (day=1; day<32; day++){
			var sumValue = 0.000;
			for (hour =0; hour<24; hour++){
				var path = "customers/" + userId+"/consumption/"+year+"/"+monthNmbr+"/"+day+"/"+hour;
				console.log("path = " + path);
				var hourVol	= snapshot.child(path).val();
				console.log("hourVol string = " + hourVol);
				if (hourVol != null){
					hourVolNmbr = parseFloat(hourVol);
					//dayHourArray[[day],[hour]] = hourVolNmbr;
					console.log("hourVol != null");
				}
				else{
					hourVolNmbr = '0.000';
					//dayHourArray[[day],[hour]] = hourVolNmbr;
					console.log("hourVol == null");
				}
				dayHourArray[[day],[hour]] = hourVolNmbr;
				console.log("dayHourArray[[" + day + "], [" + hour + "]]= " + dayHourArray[[day],[hour]]);
				sumValue = sumValue + parseFloat(dayHourArray[[day],[hour]]);
			}
			sumOfDay[day] = sumValue;
			console.log("sumOfDay[" + day +"] = " + sumOfDay[day]);
		}
		sumOfMonth = sumOfDay.reduce((a, b) => a + b, 0);
		var display = sumOfMonth.toFixed(3);
		console.log("display = " + display);
		var txt = "3";
		document.getElementById("consumption").innerHTML = "<b>" + display + " m" + txt.sup() + "</b>";		
		

		var ctx = document.getElementById('myChart').getContext('2d');
		var myChart = new Chart(ctx, {
			type: 'bar',
			data: {
				labels: [],
				datasets: [{
					label: 'Η κατανάλωση της ημέρας',
					data: [],
					backgroundColor: [],
					borderColor: [],
					borderWidth: 1
					}]
				},
			options: {
					responsive: true,
					legend: {
						display: false,
							},
					title: {
						display: true,
						text: 'Μηνιαία κατανάλωση αναλυτικά'
							},
					scales: {
						yAxes: [{
						ticks: {
							beginAtZero: true
								}
							}]
						}
					}
			});
		var dayLabel;
		var dayLabelArray = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31];
		sumOfDay.shift();								//remove the first element (0.00) 
		for (dayLabel=0; dayLabel<32; ++dayLabel){
			myChart.data.labels.push(dayLabelArray[dayLabel]);
			myChart.update();
			myChart.data.datasets.forEach((dataset) => {
				dataset.backgroundColor.push('rgba(255, 102, 0, 1)');
			});
			myChart.update();
			myChart.data.datasets.forEach((dataset) => {
				dataset.borderColor.push('rgba(255, 102, 0, 1)');
			});
			myChart.update();
			myChart.data.datasets.forEach((dataset) => {
				dataset.data.push(sumOfDay[dayLabel]);
			});
			myChart.update();
		}
		
		});
		
	
}	

function getLastUpdate(userId) {
	var ref = firebase.database().ref("customers/" + userId + "/sensors");
	ref.once("value")
		.then(function(snapshot){
		ref.on("value" , gotData , errData);
		
	});
}

function gotData(data){
	data = data.val();
	lastUpdate = data.lastupdate;
	console.log("lastUpdate= " + lastUpdate);
	document.getElementById("last_update").innerHTML = lastUpdate; //update the last update value
}

function errData(error){
	console.log(error.message , error.code);
}

