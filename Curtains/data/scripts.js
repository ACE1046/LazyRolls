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

function st(t, id, tag)
{ f=t.responseXML.getElementsByTagName(tag)[0]; if(f) document.getElementById(id).innerHTML = f.childNodes[0].nodeValue; }
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
