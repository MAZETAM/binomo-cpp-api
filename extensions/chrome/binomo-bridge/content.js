/*
* binomo-cpp-api - C ++ API client for https://binomo.com
*
* Copyright (c) 2020 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

var symbols_array = [];
var socket;
var socket_array = [];
var api_socket;

var port;

var is_socket = false;
var is_last_socket = false;

var is_api_socket = false;
var is_last_api_socket = false;

var is_error = false;
var is_last_error = false;

var is_unsubscribe = false;

function getUuid() {
    return(Date.now().toString(36)+Math.random().toString(36).substr(2,12)).toUpperCase()
}

// возвращает куки с указанным name,
// или undefined, если ничего не найдено
function getCookie(name) {
	let matches = document.cookie.match(new RegExp(
		"(?:^|; )" + name.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g, '\\$1') + "=([^;]*)"
	));
	return matches ? decodeURIComponent(matches[1]) : undefined;
}

function test22() {
	function reqListener () {
	  console.log(this.responseText);
	}

	var xdr = new XDomainRequest;
	xdr.open("GET", "https://api.binomo.com/platform/private/v3/assets?locale=ru", true); 
	xdr.setRequestHeader('Accept', 'application/json, text/plain, */*');
	xdr.setRequestHeader('Accept-Language', 'ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7');
	// xdr.setRequestHeader('Accept-Encoding', 'gzip, deflate, br');
	xdr.setRequestHeader('Device-Id', getCookie("device_id"));
	xdr.setRequestHeader('Version', 'a003e0ad');
	xdr.setRequestHeader('Device-Type', 'web');
	xdr.setRequestHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
	xdr.setRequestHeader('User-Timezone', 'Europe/Moscow');
	xdr.setRequestHeader('Authorization-Token', getCookie("authtoken"));
	xdr.setRequestHeader('Content-Type', 'application/json');
	// xdr.setRequestHeader('Sec-Fetch-Dest', 'empty'),
	// xdr.setRequestHeader('Sec-Fetch-Mode', 'cors'),
	// xdr.setRequestHeader('Sec-Fetch-Site', 'same-site'),
	// xdr.setRequestHeader('dnt', '1'),
	xdr.setRequestHeader('Origin', 'https://binomo.com');
	xdr.setRequestHeader('Access-Control-Allow-Origin', '*');
	// xdr.setRequestHeader('Connection', 'keep-alive'),
	// xdr.setRequestHeader('Referer', 'https://binomo.com/trading'),
	// xdr.setRequestHeader('access-control-request-headers', 'authorization-token,cache-control,content-type,device-id,device-type,user-timezone,version'),
	// xdr.setRequestHeader('access-control-request-method', 'GET'),
	// xdr.setRequestHeader('cache-control', 'no-cache, no-store, must-revalidate'),
	// xdr.setRequestHeader('device_id', getCookie("device_id")),
	// xdr.setRequestHeader('authorization-token', getCookie("authtoken")),
	// xdr.setRequestHeader('device-type', 'web'),
	// xdr.setRequestHeader('version', 'a003e0ad'),
	xdr.withCredentials = true;
	xdr.onreadystatechange = function() {
		if (4 == xdr.readyState) {
			if (200 != xdr.status) {
				console.log(xdr.status + ": " + xdr.statusText);
				is_error = true;
			} else {
				console.log("https://api.binomo.com/platform/private/v3/assets?locale=ru xdr.responseText out: " + xdr.responseText);
			}
		} else {
			console.log("xdr.readyState: " + xdr.readyState);
		}
	}
	xdr.onload = reqListener;
	xdr.send(); 
}

function test23() {
	function reqListener () {
	  console.log(this.responseText);
	}
	var rt = new XMLHttpRequest;
	rt.open("GET", "https://api.binomo.com/platform/private/v3/assets?locale=ru", true); 
	rt.setRequestHeader('Accept', 'application/json, text/plain, */*');
	rt.setRequestHeader('Accept-Language', 'ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7');
	// rt.setRequestHeader('Accept-Encoding', 'gzip, deflate, br');
	rt.setRequestHeader('Device-Id', getCookie("device_id"));
	rt.setRequestHeader('Version', 'a003e0ad');
	rt.setRequestHeader('Device-Type', 'web');
	rt.setRequestHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
	rt.setRequestHeader('User-Timezone', 'Europe/Moscow');
	rt.setRequestHeader('Authorization-Token', getCookie("authtoken"));
	rt.setRequestHeader('Content-Type', 'application/json');
	// rt.setRequestHeader('Sec-Fetch-Dest', 'empty'),
	// rt.setRequestHeader('Sec-Fetch-Mode', 'cors'),
	// rt.setRequestHeader('Sec-Fetch-Site', 'same-site'),
	// rt.setRequestHeader('dnt', '1'),
	rt.setRequestHeader('Origin', 'https://binomo.com');
	rt.setRequestHeader('Access-Control-Allow-Origin', '*');
	// rt.setRequestHeader('Connection', 'keep-alive'),
	// rt.setRequestHeader('Referer', 'https://binomo.com/trading'),
	// rt.setRequestHeader('access-control-request-headers', 'authorization-token,cache-control,content-type,device-id,device-type,user-timezone,version'),
	// rt.setRequestHeader('access-control-request-method', 'GET'),
	// rt.setRequestHeader('cache-control', 'no-cache, no-store, must-revalidate'),
	// rt.setRequestHeader('device_id', getCookie("device_id")),
	// rt.setRequestHeader('authorization-token', getCookie("authtoken")),
	// rt.setRequestHeader('device-type', 'web'),
	// rt.setRequestHeader('version', 'a003e0ad'),
	rt.withCredentials = true;
	rt.onreadystatechange = function() {
		if (4 == rt.readyState) {
			if (200 != rt.status) {
				console.log(rt.status + ": " + rt.statusText);
				is_error = true;
			} else {
				console.log("https://api.binomo.com/platform/private/v3/assets?locale=ru rt.responseText out: " + rt.responseText);
			}
		} else {
			console.log("rt.readyState: " + rt.readyState);
		}
	}
	rt.onload = reqListener;
	rt.send(); 
}


function injected_main() {
	console.log("Binomo Bridge launched");
	
	//var broker_domain = document.domain;
	
	console.log("device_id = " + getCookie("device_id"));
	console.log("authorization-token = " + getCookie("authtoken"));

	function connect_broker() {
		var authorization_token = getCookie("authtoken");
		var device_id = getCookie("device_id");
		socket = new WebSocket("wss://ws.binomo.com/?authtoken=" + authorization_token + "&device=web&device_id=" + device_id + "&v=2&vsn=2.0.0"), 
		socket.onopen = function() {
            is_socket = true;
			console.log("Соединение установлено."); 
			if(is_api_socket) {
				api_socket.send('{"event":"socket","body":{"status":"open","authtoken":"' + authorization_token + '","device_id":"' + device_id + '"}}');
			}
        }, 
		socket.onclose = function(t) {
			is_socket = false;
            if(is_api_socket) {
				api_socket.send('{"event":"init","body":{"connection_status":"reconnecting"}}');
				connect_broker(); 
			}
			t.wasClean ? console.log("Соединение закрыто чисто") : console.log("Обрыв соединения"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
        }, 
		socket.onmessage = function(t) {
			console.log("Получены данные" + t.data);
			if(is_api_socket) {
				api_socket.send(t.data);
			}		
        }, 
		socket.onerror = function(t) {
			is_socket = false;
			socket.close();
			if(is_api_socket) {
				api_socket.send('{"event":"init","body":{"connection_status":"error"}}');
			}
			console.log("Ошибка " + t.message);
        }
    }
	
	function connect_api() {
		api_socket = new WebSocket("ws://localhost:" + port + "/binomo-api"), 
		api_socket.onopen = function() {
			is_api_socket = true;
			connect_broker();
			console.log("Соединение с сервером API установлено.");
		}, api_socket.onclose = function(t) {
			is_api_socket = false;
			
			/* закрываем соединение с брокером */
			if(is_socket) socket.close();
			is_socket = false;
			
			/* пробуем переподключиться*/
			connect_api(); 
			
			t.wasClean ? console.log("Соединение с сервером API закрыто чисто") : console.log("Обрыв соединения с сервером API"), 
			console.log("Код: " + t.code + " причина: " + t.reason);
		}, api_socket.onmessage = function(t) {
			console.log("Получены данные от сервера API: " + t.data); 
			
			// var api_message = JSON.parse(t.data);
			
			if(is_socket) {
				socket.send(t.data);
			}
		}, api_socket.onerror = function(t) {
			is_api_socket = false;
			
			console.log("Ошибка (сервер API) " + t.message);
			
			if(is_socket) socket.close();
			is_socket = false;
		}
    }
	
	chrome.storage.local.get("binomo_bridge_1v0_ws_port", function(result) {
		if(result.primexbt_bridge_1v0_ws_port) {
			port = result.primexbt_bridge_1v0_ws_port; 
		} else {
			port = 8082;
		}
		console.log('port: ' + port);
		connect_api();
    });
	
	chrome.storage.onChanged.addListener(function(changes, namespace) {
		for (var key in changes) {
			var storageChange = changes[key];
			console.log('Storage key "%s" in namespace "%s" changed. ' +
			'Old value was "%s", new value is "%s".',
			key,
			namespace,
			storageChange.oldValue,
			storageChange.newValue);
			if(key == "binomo_bridge_1v0_ws_port") {
				port = storageChange.newValue;
				if(is_api_socket) {
					is_api_socket.close();
					connect_api();
				}
				console.log('port: ' + port);
			}
		}
	});
}

function update_10_second() { 
	// test22();
	// test23();
	console.log("binomo bridge is work");
}

setInterval(update_10_second, 10000);// запускать функцию каждую секунду

function try_again() {
	injected_main()
}

try_again();