
//Работа с webSocket сервером
function wsConnect(wsIP) {
	let wsAdress = 'ws://'+wsIP+':81/'+data['page']+'.htm';
	console.log("wsAdress=", wsAdress);
	ws = new WebSocket(wsAdress, ['arduino']);
	
	ws.onopen = function(e) {
  		console.log("WS onConnected");
  		flagWsState = 1;
  	};

	ws.onclose = function(e) {
		console.log('WS is closed.', e.reason);
		flagWsState = 0;
	};

	ws.onerror = function (error) {
		flagWsState = 0;
	};

	ws.onmessage = function (e) {
		console.log('WS FROM Server: ', e.data);
		receivedDataProcessing (e.data);
	};
}



function startSendData(command) {
	switch (command) {
		case "JSON":
			sendFinishData(JSON.stringify(dataSend));
			break;
		case "RESET":
			sendFinishData('RESET');
			break;
	}

	function sendFinishData(dat){
		if (flagWsState==1){
			console.log('WS TO Server: ', dat);
			ws.send(dat);
		}
	};

}




function receivedDataProcessing(strJson){
	try {
		let obj=JSON.parse(strJson);
		//перебираем все элементы в obj, ищем элементы с именами переменных в
		//структурах data и dataSend и сохраняем их содержимое
		for (x in obj){
			if (data[x]!=null){
				data[x]=obj[x];
			} 
			else if (dataSend[x]!=null){
				dataSend[x]=obj[x];
			}
		}
		updateAllPage();
	} catch (e){
		console.log(e.message); 
	}
	console.log(dataSend);
	console.log(data); 
}


//Старт скрипта
var deviceIp="192.168.1.235";
var flagWsState = 0;   //оостояние соединения с webSocket сервером

//Определение IP-адреса контроллера с которого загружена страница
if (location.host){
	deviceIp = location.host;
	console.log('deviceIp=', deviceIp);
}

//соединение с webSocket сервером на контроллере
if (deviceIp!="")   wsConnect(deviceIp);
