function consoleLog(message, color = "black") {
  let msg = null;
  if (color == "black") {
    console.log(message);
  } else {
    msg = "%c" + message;
    msgColor = "color: " + color;
    console.log(msg, msgColor);
  }
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

class xplrHpgHtmlBuilder {
  constructor() {
    this.context = "";
  }

  get navbar() {
    return this.build("navbar", null);
  }

  get dvcStatus() {
    return this.build("dvcStatus", null);
  }

  get ssidScanResults() {
    return this.build("ssidScanResults", null);
  }

  get wifiConnected() {
    return this.build("wifiConnected", null);
  }

  get wifiOffline() {
    return this.build("wifiOffline", null);
  }

  get wifiNotSet() {
    return this.build("wifiNotSet", null);
  }

  get ppNotSet() {
    return this.build("ppNotSet", null);
  }

  get ppConfigured() {
    return this.build("ppConfigured", null);
  }

  get ppConnected() {
    return this.build("ppConnected", null);
  }

  get gnssNoSignal() {
    return this.build("gnssNoSignal", null);
  }

  get gnss3dFix() {
    return this.build("gnss3dFix", null);
  }

  get gnssDgnss() {
    return this.build("gnssDgnss", null);
  }

  get gnssRtkFix() {
    return this.build("gnssRtkFix", null);
  }

  get gnssRtkFloat() {
    return this.build("gnssRtkFloat", null);
  }

  get gnssDeadReckon() {
    return this.build("gnssDeadReckon", null);
  }

  build(element, value) {
    var ctx = null;

    if (element == "navbar") {
      ctx =
        '\
        <nav class="navbar navbar-expand-lg bg-body-tertiary" id="navbarMenu">\
          <div class="container-fluid">\
              <a class="navbar-brand" href="/">\
                <img src="/static/img/ublox_logo.svg" alt="Logo" width="128" height="50"\
                class="d-inline-block align-text-top">\
              </a>\
            <button class="navbar-toggler ml-auto hidden-sm-up float-xs-right" type="button" data-bs-toggle="collapse" data-bs-target="#navbarText" aria-controls="navbarText" aria-expanded="false" aria-label="Toggle navigation">\
              <span class="navbar-toggler-icon"></span>\
            </button>\
            <div class="collapse navbar-collapse" id="navbarText">\
              <ul class="navbar-nav me-auto mb-2 mb-lg-0">\
                <li class="nav-item">\
                  <a class="nav-link" href="/index.html">Home</a>\
                </li>\
                <li class="nav-item">\
                  <a class="nav-link" href="/settings.html">Settings</a>\
                </li>\
                <li class="nav-item">\
                  <a class="nav-link" href="/tracker.html"><span>Live Tracker</span></a>\
                </li>\
              </ul>\
            </div>\
          </div>\
        </nav>';
      this.context = ctx;
    } else if (element == "dvcStatus") {
      ctx =
        '\
        <ul class="list-group">\
          <li class="list-group-item d-flex justify-content-between align-items-center" id="jsDvcStatusWifi">\
            Wi-Fi\
            <span class="badge text-bg-danger">Not Set</span>\
          </li>\
          <li class="list-group-item d-flex justify-content-between align-items-center" id="jsDvcStatusPointPerfect">\
            Point Perfect\
            <span class="badge text-bg-danger">Not set</span>\
          </li>\
          <li class="list-group-item d-flex justify-content-between align-items-center" id="jsDvcStatusGnss">\
            GNSS Fix\
            <span class="badge text-bg-danger">No signal</span>\
          </li>\
        </ul>';
      this.context = ctx;
    } else if (element == "wifiConnected") {
      ctx =
        '\
      Wi-Fi\
      <span class="badge text-bg-success">Connected</span>';
      this.context = ctx;
    } else if (element == "wifiOffline") {
      ctx =
        '\
      Wi-Fi\
      <span class="badge text-bg-warning">Offline</span>';
      this.context = ctx;
    } else if (element == "wifiNotSet") {
      ctx =
        '\
      Wi-Fi\
      <span class="badge text-bg-danger">Not set</span>';
      this.context = ctx;
    } else if (element == "ppNotSet") {
      ctx =
        '\
      Point Perfect\
      <span class="badge text-bg-danger">Not set</span>';
      this.context = ctx;
    } else if (element == "ppConfigured") {
      ctx =
        '\
      Point Perfect\
      <span class="badge text-bg-secondary">Configured</span>';
      this.context = ctx;
    } else if (element == "ppConnected") {
      ctx =
        '\
      Point Perfect\
      <span class="badge text-bg-success">Connected</span>';
      this.context = ctx;
    } else if (element == "gnssNoSignal") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-danger">No signal</span>';
      this.context = ctx;
    } else if (element == "gnss3dFix") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-warning">2D/3D</span>';
      this.context = ctx;
    } else if (element == "gnssDgnss") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-light">DGNSS</span>';
      this.context = ctx;
    } else if (element == "gnssRtkFix") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-success">RTK Fixed</span>';
      this.context = ctx;
    } else if (element == "gnssRtkFloat") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-info">RTK Float</span>';
      this.context = ctx;
    } else if (element == "gnssDeadReckon") {
      ctx =
        '\
      GNSS Fix\
      <span class="badge text-bg-primary">Dead Reckon</span>';
      this.context = ctx;
    } else if (element == "ssidScanResults") {
      if (value.rsp == "dvcSsidScan") {
        ctx = "";
        for (const msg of value.scan) {
          ctx +=
            '\
            <div class="row">\
            <div class="col">';
          ctx += msg + "</div>";
          ctx += "</div>";
        }
        ctx += "</div>";
      }
      this.context = ctx;
    } else {
      this.context = null;
    }
    return this.context;
  }

  updateSsidScanTable(value) {
    return this.build("ssidScanResults", value);
  }

  updateDvcInfo(value) {
    return this.build("dvcInfo", value);
  }
}

class xplrHpgData {
  constructor() {
    this.ssid = null;
    this.password = null;
    this.rootCa = null;

    this.ppId = null;
    this.ppCert = null;
    this.ppKey = null;
    this.ppRegion = null;

    this.dvcConfiguredAlert = false;

    this.gnss = JSON.parse(
      '{"latitude":null, \
        "longitude":null, \
        "altitude":null, \
        "speed":null, \
        "accuracy":null, \
        "fixType":null, \
        "timestamp":null, \
        "gMap":null}'
    );

    this.diagnostics = JSON.parse(
      '{"wifi":null, \
        "thingstream":null, \
        "gnss":null}'
    );

    this.info = JSON.parse(
      '{"ssid":null, \
        "ip":null, \
        "host":null, \
        "uptime":null, \
        "timeToFix":null, \
        "mqttTraffic":null, \
        "accuracy":null, \
        "fwVersion":null}'
    );

    this.socket = null;
    this.socketStatus = 0;
    this.socketBufferIn = [];
  }

  get socketStatus() {
    return this._socketStatus;
  }

  set socketStatus(value) {
    this._socketStatus = value;
  }

  get ssid() {
    return this._ssid;
  }

  set ssid(value) {
    this._ssid = value;
  }

  get password() {
    return this._password;
  }

  set password(value) {
    this._password = value;
  }

  get rootCa() {
    return this._rootCa;
  }

  set rootCa(value) {
    this._rootCa = value;
  }

  get ppId() {
    return this._ppId;
  }

  set ppId(value) {
    this._ppId = value;
  }

  get ppCert() {
    return this._ppCert;
  }

  set ppCert(value) {
    this._ppCert = value;
  }

  get ppKey() {
    return this._ppKey;
  }

  set ppKey(value) {
    this._ppKey = value;
  }

  get ppRegion() {
    return this._ppRegion;
  }

  set ppRegion(value) {
    this._ppRegion = value;
  }

  get dvcConfiguredAlert() {
    return this._dvcConfiguredAlert;
  }

  set dvcConfiguredAlert(value) {
    this._dvcConfiguredAlert = value;
  }
}

class xplrHpgMap {
  constructor() {
    this.map = null;
    this.track = null;
    this.point = null;
    this.position = JSON.parse('{"lon":null, "lat":null}');
  }

  get map() {
    return this._map;
  }

  set map(value) {
    this._map = value;
  }

  get track() {
    return this._track;
  }

  set track(value) {
    this._track = value;
  }

  get point() {
    return this._point;
  }

  set point(value) {
    this._point = value;
  }

  buildMap(longitude, latitude) {
    let el = document.getElementById("map");
    if (ol !== undefined) {
      el.removeAttribute("hidden");
      this.position.lon = longitude;
      this.position.lat = latitude;
      const pos = ol.proj.fromLonLat([this.position.lon, this.position.lat]);
      this.track = new ol.Feature({
        geometry: new ol.geom.LineString([]),
      });
      this.track.setStyle(
        new ol.style.Style({
          stroke: new ol.style.Stroke({
            color: "rgba(255,110,89,0.7)",
            width: 3,
            lineCap: "round",
          }),
        })
      );
      let svg =
        '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="22" y1="12" x2="18" y2="12"></line><line x1="6" y1="12" x2="2" y2="12"></line><line x1="12" y1="6" x2="12" y2="2"></line><line x1="12" y1="22" x2="12" y2="18"></line></svg>';
      let icon = new ol.style.Icon({
        color: "#ff6e59",
        opacity: 1,
        src: "data:image/svg+xml;utf8," + svg,
        anchor: [0.5, 0.5],
        anchorXUnits: "fraction",
        anchorYUnits: "fraction",
      });
      this.point = new ol.Feature({ geometry: new ol.geom.Point(pos) });
      this.point.setStyle(new ol.style.Style({ image: icon }));
      this.map = new ol.Map({
        target: "map",
        controls: ol.control.defaults.defaults().extend([new ol.control.ScaleLine({ units: "metric" })]),
        layers: [
          new ol.layer.Tile({
            source: new ol.source.OSM(),
          }),
          new ol.layer.Vector({
            source: new ol.source.Vector({
              features: [this.point, this.track],
            }),
          }),
        ],
        view: new ol.View({
          center: ol.proj.fromLonLat(pos),
          zoom: 16,
        }),
      });
      this.map.getView().setCenter(pos);
    }
  }

  updateMapLocation(longitude, latitude) {
    if (this.map != null) {
      const pos = ol.proj.fromLonLat([longitude, latitude]);
      this.map.getView().setCenter(pos);
      this.track.getGeometry().appendCoordinate(pos);
      this.point.getGeometry().setCoordinates(pos);
    }
  }
}

class xplrHpg {
  constructor(url, page) {
    this.data = new xplrHpgData();
    this.htmlCtx = new xplrHpgHtmlBuilder();
    this.mapInstance = new xplrHpgMap();
    this.url = url;
    this.pageActive = page;
    this.loopOnce = null;
  }

  showNavbar() {
    const ctx = this.htmlCtx.navbar;
    var element = document.getElementById("jsNavbar");
    element.innerHTML = ctx;
  }

  eventDisplay(event, state, value) {
    var ctx = null;

    if (event == "dvcStatus") {
      ctx = this.htmlCtx.dvcStatus;
      var element = document.getElementById("jsDvcStatus");
      element.innerHTML = ctx;
    } else if (event == "jsDvcInfo") {
      if (state == "update") {
        document.getElementById("jsDvcInfoSsid").value = value.ssid;

        document.getElementById("jsDvcInfoIP").value = value.ip;

        if (value.host == "n/a") {
          document.getElementById("jsDvcInfoHostname").value = value.host;
        } else {
          document.getElementById("jsDvcInfoHostname").value = value.host + ".local";
        }

        document.getElementById("jsDvcInfoUptime").value = value.uptime;

        if (value.timeToFix === undefined) {
          value.timeToFix = "n/a";
        }
        document.getElementById("jsDvcInfoTimeToFix").value = value.timeToFix;

        if (value.accuracy == 0) {
          document.getElementById("jsDvcInfoAccuracy").value = "n/a";
        } else {
          document.getElementById("jsDvcInfoAccuracy").value = (value.accuracy * 1e-4).toFixed(3) + "m";
        }

        if (value.mqttTraffic === undefined) {
          value.mqttTraffic = "n/a";
        }

        document.getElementById("jsDvcInfoMqttTraffic").value = value.mqttTraffic;
        document.getElementById("jsDvcInfoFwVersion").value = value.fwVersion;
      }
    } else if (event == "jsDvcStatusWifi") {
      if (this.pageActive == "status") {
        switch (state) {
          case "notSet":
            ctx = this.htmlCtx.wifiNotSet;
            var element = document.getElementById("jsDvcStatusWifi");
            element.innerHTML = ctx;
            break;
          case "offline":
            ctx = this.htmlCtx.wifiOffline;
            var element = document.getElementById("jsDvcStatusWifi");
            element.innerHTML = ctx;
            break;
          case "connected":
            ctx = this.htmlCtx.wifiConnected;
            var element = document.getElementById("jsDvcStatusWifi");
            element.innerHTML = ctx;
            break;
          default:
            break;
        }
      }
      this.data.diagnostics.wifi = state;
    } else if (event == "jsDvcStatusPointPerfect") {
      if (this.pageActive == "status") {
        switch (state) {
          case "notSet":
            ctx = this.htmlCtx.ppNotSet;
            var element = document.getElementById("jsDvcStatusPointPerfect");
            element.innerHTML = ctx;
            break;
          case "configured":
            ctx = this.htmlCtx.ppConfigured;
            var element = document.getElementById("jsDvcStatusPointPerfect");
            element.innerHTML = ctx;
            break;
          case "connected":
            ctx = this.htmlCtx.ppConnected;
            var element = document.getElementById("jsDvcStatusPointPerfect");
            element.innerHTML = ctx;
            break;
          default:
            break;
        }
      }
      this.data.diagnostics.thingstream = state;
    } else if (event == "jsDvcStatusGnss") {
      if (this.pageActive == "status") {
        switch (state) {
          case "noSignal":
            ctx = this.htmlCtx.gnssNoSignal;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          case "3dFix":
            ctx = this.htmlCtx.gnss3dFix;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          case "dgnss":
            ctx = this.htmlCtx.gnssDgnss;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          case "rtkFixed":
            ctx = this.htmlCtx.gnssRtkFix;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          case "rtkFloat":
            ctx = this.htmlCtx.gnssRtkFloat;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          case "deadReckon":
            ctx = this.htmlCtx.gnssDeadReckon;
            var element = document.getElementById("jsDvcStatusGnss");
            element.innerHTML = ctx;
            break;
          default:
            break;
        }
      }
      this.data.diagnostics.gnss = state;
    } else if (event == "jsSsidScan") {
      switch (state) {
        case "update":
          ctx = this.htmlCtx.updateSsidScanTable(value);
          var element = document.getElementById("jsSsidScanResults");
          element.innerHTML = ctx;
          break;
        default:
          break;
      }
    }
  }

  wsConnect() {
    var self = this;
    if (this.data.socketStatus != 1) {
      if (this.url === undefined) {
        this.url = "ws://192.168.4.1/xplrHpg";
      }

      consoleLog("Creating Websocket server @ " + this.url);
      this.data.socket = new WebSocket(this.url);

      this.data.socket.onopen = function (e) {
        consoleLog("[xplrHpg] Websocket connection established", "green");
      };

      this.data.socket.onmessage = function (event) {
        consoleLog("[xplrHpg] Websocket Data received:" + event.data);
        const jObj = JSON.parse(event.data);
        self.data.socketBufferIn.push(jObj);
        self.wsParseReq();
      };

      this.data.socket.onclose = function (event) {
        if (event.wasClean) {
          alert(`Websocket connection closed cleanly, code=${event.code} reason=${event.reason}`);
        } else {
          // e.g. server process killed or network down
          // event.code is usually 1006 in this case
          alert("Websocket connection died");
        }
      };

      this.data.socket.onerror = function (error) {
        consoleLog(error, "red");
        alert(`[error]`);
      };

      setInterval(() => {
        if (this.data.socket.readyState == 1 && this.data.socketStatus == 0) {
          this.data.socketStatus = 1;
          consoleLog("[xplrHpg] Websocket ready", "green");
        } else if (this.data.socket.readyState > 1) {
          this.data.socketStatus = 0;
        }
      }, 1000);
    }
  }

  wsRequestDvcStatus() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcStatus"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device status req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcInfo() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcInfo"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device info req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcSsidScan() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcSsidScan"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device ssid scan req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcLocation() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcLocation"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device location req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateWifiCreds(ssid, password) {
    this.data.ssid = ssid;
    this.data.password = password;
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcWifiSet",';
      let ssid = '"ssid":' + '"' + this.data.ssid + '",';
      let pwd = '"pwd":' + '"' + this.data.password + '"';
      let msg = "{" + cmd + ssid + pwd + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set wifi creds req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateThingstreamPpId(id) {
    this.data.ppId = id;
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcThingstreamPpIdSet",';
      let id = '"id":' + '"' + this.data.ppId + '"';
      let msg = "{" + cmd + id + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set thingstream pp id req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateThingstreamPpRootCa(cert) {
    this.data.rootCa = cert;
    let escCert = this.data.rootCa.replaceAll(/(\r\n|\n|\r)/gm, "\\n");
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcThingstreamPpRootCaSet",';
      let value = '"root":' + '"' + escCert + '"';
      let msg = "{" + cmd + value + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set thingstream pp rootCa req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateThingstreamPpCert(cert) {
    this.data.ppCert = cert;
    let escCert = this.data.ppCert.replaceAll("\n", "\\n");
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcThingstreamPpCertSet",';
      let cert = '"cert":' + '"' + escCert + '"';
      let msg = "{" + cmd + cert + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set thingstream pp cert req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateThingstreamPpKey(key) {
    this.data.ppKey = key;
    let escCert = this.data.ppKey.replaceAll("\n", "\\n");
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcThingstreamPpKeySet",';
      let key = '"key":' + '"' + escCert + '"';
      let msg = "{" + cmd + key + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set thingstream pp key req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsUpdateThingstreamPpRegion(region) {
    this.data.ppRegion = region;
    if (this.data.socketStatus == 1) {
      let cmd = '"req":"dvcThingstreamPpRegionSet",';
      let ppRegion = '"region":' + '"' + this.data.ppRegion + '"';
      let msg = "{" + cmd + ppRegion + "}";
      const jObj = JSON.parse(msg);
      consoleLog("[xplrHpg] Device set thingstream pp key req msg:" + JSON.stringify(jObj), "cyan");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcReboot() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcReboot"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device reboot req msg:" + JSON.stringify(jObj), "yellow");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcErase() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcErase"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device erase req msg:" + JSON.stringify(jObj), "yellow");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcEraseWifi() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcEraseWifi"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device erase wi-fi creds req msg:" + JSON.stringify(jObj), "yellow");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcEraseThingstream() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcEraseThingstream"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device erase thingstream creds req msg:" + JSON.stringify(jObj), "yellow");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcRequest failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsRequestDvcMessage() {
    if (this.data.socketStatus == 1) {
      let text = '{"req":"dvcMessage"}';
      const jObj = JSON.parse(text);
      consoleLog("[xplrHpg] Device req msg:" + JSON.stringify(jObj), "yellow");
      this.data.socket.send(JSON.stringify(jObj));
    } else {
      consoleLog("[xplrHpg] Websocket dvcMessage failed, socket state:" + this.data.socket.readyState, "red");
    }
  }

  wsParseReq() {
    if (this.data.socketStatus == 1) {
      let index = 0;
      for (const msg of this.data.socketBufferIn) {
        index++;
        switch (msg.rsp) {
          case "dvcStatus":
            if (msg.wifi == "-1") {
              this.eventDisplay("jsDvcStatusWifi", "notSet", null);
            } else if (msg.wifi == "0") {
              this.eventDisplay("jsDvcStatusWifi", "offline", null);
            } else {
              this.eventDisplay("jsDvcStatusWifi", "connected", null);
            }

            if (msg.thingstream == "-1") {
              this.eventDisplay("jsDvcStatusPointPerfect", "notSet", null);
            } else if (msg.thingstream == "0") {
              this.eventDisplay("jsDvcStatusPointPerfect", "configured", null);
            } else {
              this.eventDisplay("jsDvcStatusPointPerfect", "connected", null);
            }

            if (msg.gnss == "-1" || msg.gnss == "0") {
              this.eventDisplay("jsDvcStatusGnss", "noSignal", null);
            } else if (msg.gnss == "1") {
              this.eventDisplay("jsDvcStatusGnss", "3dFix", null);
            } else if (msg.gnss == "2") {
              this.eventDisplay("jsDvcStatusGnss", "dgnss", null);
            } else if (msg.gnss == "4") {
              this.eventDisplay("jsDvcStatusGnss", "rtkFixed", null);
            } else if (msg.gnss == "5") {
              this.eventDisplay("jsDvcStatusGnss", "rtkFloat", null);
            } else if (msg.gnss == "6") {
              this.eventDisplay("jsDvcStatusGnss", "deadReckon", null);
            } else {
              this.eventDisplay("jsDvcStatusGnss", "noSignal", null);
            }

            this.data.socketBufferIn.splice(--index, 1);

            break;
          case "dvcInfo":
            this.data.info.ssid = msg.ssid;
            this.data.info.ip = msg.ip;
            this.data.info.host = msg.host;
            this.data.info.uptime = msg.uptime;
            this.data.info.timeToFix = msg.timeToFix;
            this.data.info.accuracy = msg.accuracy;
            this.data.info.mqttTraffic = msg.mqttTraffic;
            this.data.info.fwVersion = msg.fwVersion;
            this.eventDisplay("jsDvcInfo", "update", msg);
            this.data.socketBufferIn.splice(--index, 1);
            break;
          case "dvcSsidScan":
            this.eventDisplay("jsSsidScan", "update", msg);
            this.data.socketBufferIn.splice(--index, 1);
            break;
          case "dvcLocation":
            this.data.gnss.latitude = msg.lat;
            this.data.gnss.longitude = msg.lon;
            this.data.gnss.altitude = msg.alt;
            this.data.gnss.speed = msg.speed;
            this.data.gnss.accuracy = msg.accuracy;
            this.data.gnss.fixType = msg.type;
            this.data.gnss.timestamp = msg.timestamp;
            this.data.gnss.gMap = msg.gMap;
            if (this.data.diagnostics.wifi == "connected") {
              if (this.data.gnss.latitude != null && this.data.gnss.longitude != null) {
                let lon = this.data.gnss.longitude;
                let lat = this.data.gnss.latitude;
                this.mapInstance.updateMapLocation(lon, lat);
              }
            }

            let fixType = null;
            switch (msg.type) {
              case -1:
              case 0:
                fixType = "noSignal";
                break;
              case 1:
                fixType = "3D-Fixed";
                break;
              case 2:
                fixType = "DGNSS";
                break;
              case 4:
                fixType = "RTK-Fixed";
                break;
              case 5:
                fixType = "RTK-Float";
                break;
              case 6:
                fixType = "Dead Reckon";
                break;
            }

            let logMsg =
              msg.timestamp + " " + "WLAN" + " " + msg.accuracy + " " + msg.lat + " " + msg.lon + " " + fixType;

            //https://www.regexpal.com/
            const log = logMsg.match(
              //time        src acc      lat          lon          fix
              /^\d+:\d+:\d+ \w+ (\d+(\.\d+)?) (-?\d+\.\d+) (-?\d+\.\d+) (\S+)/
            );

            if (log != null) {
              this.trackerLog(log[0]);
            } else {
              consoleLog("Error matching gnss info!", "red");
            }

            this.data.socketBufferIn.splice(--index, 1);
            break;
          default:
            break;
        }
      }
    } else {
      consoleLog("Socket disconnected, nothing to parse", "red");
    }
  }

  trackerLog(message, color = "black") {
    let output = document.querySelector("#gnssLog");
    if (output != null) {
      const el = document.createElement("div");
      el.innerHTML = message;
      el.style.color = color;
      output.append(el);
      output.scrollTop = output.scrollHeight;
    }
  }

  isDeviceConfigured() {
    if (
      this.data.ssid != null &&
      this.data.password != null &&
      this.data.rootCa != null &&
      this.data.ppId != null &&
      this.data.ppCert != null &&
      this.data.ppKey != null &&
      this.data.ppRegion != null
    ) {
      return true;
    } else {
      return false;
    }
  }

  wsLoop(page) {
    if (this.data.socketStatus == 1) {
      if (page == "dvcStatus") {
        this.wsRequestDvcStatus();
        this.wsRequestDvcInfo();
      } else if (page == "dvcTracker") {
        this.wsRequestDvcStatus();
        this.wsRequestDvcLocation();
        if (this.data.diagnostics.wifi == "connected") {
          if (this.loopOnce == null) {
            if (this.data.gnss.latitude != null && this.data.gnss.longitude != null) {
              let lon = this.data.gnss.longitude;
              let lat = this.data.gnss.latitude;
              this.mapInstance.buildMap(lon, lat);
            } else {
              this.mapInstance.buildMap(23.809275664375367, 38.04805120936767);
            }
            this.loopOnce = "done";
          }
        }
      } else if (page == "dvcConfig") {
        if (this.loopOnce == null && this.isDeviceConfigured()) {
          this.loopOnce = "done";
          alert("Device credentials configured.\nDevice will now reboot in STA mode.");
          sleep(2000).then(() => {
            this.wsRequestDvcReboot();
          });
        }
      }
    }
  }
}
