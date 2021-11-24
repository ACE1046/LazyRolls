var timerId;
var timeout=1000;
var active;
window.onfocus = function() { active = true; clearTimeout(timerId); timeout=500; GetStatus(); };
window.onblur = function() { active = false; clearTimeout(timerId); };


function HideS(s)
{
var t=document.getElementById(s);
if (t) t.className=s+" hide";
}
function ShowInfo()
{
document.getElementById("info").className="info show";
HideS("settings");
HideS("alarms");
HideS("main");
HideS("log");
return false;
}
function ShowSettings()
{
	e=document.getElementById("settings");
	if (e)
	{
		e.className="settings show";
		HideS("info");
		return false;
	} else
		return true;
}
function ShowAlarms()
{
	e=document.getElementById("alarms");
	if (e)
	{
		e.className="alarms show";
		HideS("info");
		return false;
	} else
		return true;
}
function ShowLog()
{
	e=document.getElementById("log");
	if (e)
	{
		e.className="log show";
		HideS("info");
		return false;
	} else
		return true;
}
function ShowMain()
{
	e=document.getElementById("main");
	if (e)
	{
		e.className="main show";
		HideS("info");
		return false;
	} else
		location.href='/';
}
function UpdateLog()
{
	var request = new XMLHttpRequest();
	request.onreadystatechange = function()
	{
		if (this.readyState == 4 && this.status == 200)
		{
			var rows = document.getElementById('log_table').getElementsByTagName('tr'), index;

			for (index = rows.length - 1; index >= 0; index--)
			{
				if (rows[index].className != 'sect_name')
					rows[index].parentNode.removeChild(rows[index]);
			}
			rows[0].insertAdjacentHTML("afterend", this.responseText);
		}
	}
	// send HTTP GET request
	request.open("GET", "log?table");
	request.send(null);
}

function st(t, id, tag)
{
	f=t.responseXML.getElementsByTagName(tag)[0]; 
	h = document.getElementById(id);
	if(f && h) h.innerHTML = f.childNodes[0].nodeValue; 
}
function GetStatus()
{
	if (active) {
	nocache = "&nocache=" + Math.random() * 1000000;
	var request = new XMLHttpRequest();
	request.onreadystatechange = function()
	{
	if (this.readyState == 4) {
	if (this.status == 200) {
	if (this.responseXML != null) {
	st(this, "time", 'Time');
	st(this, "uptime", 'UpTime');
	st(this, "RSSI", 'RSSI');
	st(this, "pos", 'Now');
	st(this, "dest", 'Dest');
	st(this, "switch", 'End1');
	st(this, "mqtt", 'MQTT');
	st(this, "voltage", 'Voltage');
	st(this, "led_mode", 'Mode');
	st(this, "led_level", 'Level');
	lc=document.getElementById("log_count").innerHTML;
	st(this, "log_count", 'Log');
	if (document.getElementById("log_count").innerHTML != lc &&
		document.getElementById("log")) UpdateLog();
	if (document.getElementById("pos").innerHTML != document.getElementById("dest").innerHTML)
	timeout=500;
	else
	timeout=5000;
	}
	}
	}
}
// send HTTP GET request
request.open("GET", "xml");
request.send(null);
	} timerId = setTimeout('GetStatus()', timeout);
}
function Call(url)
{
clearTimeout(timerId);
var xhttp = new XMLHttpRequest();
xhttp.open("GET", url);
xhttp.send();
timeout=500;
GetStatus();
}
function Open() { Call("set?pos=0"); return false; }
function Close() { Call("set?pos=100"); return false; }
function Steps(s) { Call("set?steps="+s); return false; }
function StepsOvr(s) { Call("set?stepsovr="+s); return false; }
function Stop() { Call("stop"); return false; }
function TestPreset(p) { var s=document.getElementById(p).value;var m=document.getElementById("length").value;if (Number(s)>Number(m)) { s=m; document.getElementById(p).value=s; } StepsOvr(s); return false; }
function SetPreset(p) { document.getElementById(p).value = document.getElementById("pos").innerHTML; }

function Test(dir)
{
document.getElementById("btn_up").disabled=true;
document.getElementById("btn_dn").disabled=true;
pinout=document.getElementById("pinout").value;
reversed=document.getElementById("reversed").value;
delay=document.getElementById("delay").value;
var xhttp = new XMLHttpRequest();
xhttp.onreadystatechange = function() {
if (this.readyState == 4 && this.status == 200) {
document.getElementById("btn_up").disabled=false;
document.getElementById("btn_dn").disabled=false;
document.getElementById("pos").innerHTML=this.responseText;
document.getElementById("dest").innerHTML=this.responseText;
	}};
url="test?pinout="+pinout+"&reversed="+reversed+"&delay="+delay;
if (dir==1) url=url+"&up=1"; else url=url+"&down=1";
xhttp.open("GET", url, true);
xhttp.send();
}
function TestUp() { Test(1); }
function TestDown() { Test(0); }


function PinChange() {
var slave=document.getElementById('slave');
var btn_pin=document.getElementById('btn_pin');
var aux_pin=document.getElementById('aux_pin');
if (!slave || !btn_pin || !aux_pin) return;
if (aux_pin.selectedIndex == btn_pin.selectedIndex) aux_pin.selectedIndex=0;
var op = btn_pin.getElementsByTagName("option");
for (var i = 1; i < op.length; i++) op[i].disabled = (i == aux_pin.selectedIndex);
op = aux_pin.getElementsByTagName("option");
for (var i = 1; i < op.length; i++) op[i].disabled = (i == btn_pin.selectedIndex);

if (slave.selectedIndex > 1) 
{
document.getElementById('pin_RX').disabled = true;
document.getElementById('aux_RX').disabled = true;
}

var dis = false;
if (btn_pin.options[btn_pin.selectedIndex].id == 'pin_RX') dis = true;
if (aux_pin.options[aux_pin.selectedIndex].id == 'aux_RX') dis = true;
var op = document.getElementById('slave').getElementsByTagName("option");
for (var i = 2; i < op.length; i++) op[i].disabled = dis;
}
