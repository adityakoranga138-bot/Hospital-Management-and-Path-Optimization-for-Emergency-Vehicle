const views = {
  overview: ["Overview", "Live occupancy, triage queue and hospital capacity from HospitalSystem."],
  map: ["City Map & Routes", "Node names come from CityMap. The highlighted path is the Dijkstra result from C++."],
  emergency: ["Register Emergency", "Creates a Patient and pushes it onto the existing min-heap dispatch queue."],
  dispatch: ["Ambulance Dispatch", "Calls processNextDispatch(): shortest path, fare, ETA, bed assignment, file save."],
  wards: ["Ward Beds", "Admitted patient list from displayBeds(). Capacity is MAX_BEDS = 100."],
  pharmacy: ["Pharmacy", "Inventory loaded from pharmacy_stock.txt by the C++ backend."],
  discharge: ["Discharge & Billing", "Runs dischargePatient() and shows the invoice computed in C++."]
};

const NODE_LAYOUT = {
  0: [80, 60],
  1: [220, 70],
  2: [90, 180],
  3: [240, 190],
  4: [360, 120],
  5: [250, 300],
  6: [470, 150],
  7: [400, 280],
  8: [520, 330],
  9: [580, 200]
};

let locations = [];
let lastRoute = [];

function $(id) { return document.getElementById(id); }

function showAlert(message, ok) {
  const el = $("alert");
  el.textContent = message;
  el.classList.remove("hidden", "ok", "err");
  el.classList.add(ok ? "ok" : "err");
}

async function api(path, options) {
  const res = await fetch(path, options);
  const data = await res.json().catch(function () { return { ok: false, error: "Invalid JSON from server" }; });
  if (!res.ok || data.ok === false) {
    throw new Error(data.error || ("HTTP " + res.status));
  }
  return data;
}

function fillLocationSelects() {
  const routeFrom = $("route-from");
  const emPickup = $("em-pickup");
  routeFrom.innerHTML = "";
  emPickup.innerHTML = "";
  locations.forEach(function (loc) {
    if (loc.isHospital) return;
    const opt1 = document.createElement("option");
    opt1.value = loc.id;
    opt1.textContent = loc.id + " — " + loc.name;
    routeFrom.appendChild(opt1);
    emPickup.appendChild(opt1.cloneNode(true));
  });
}

function drawMap(routeNames) {
  const svg = $("city-map");
  svg.innerHTML = "";
  const byName = {};
  locations.forEach(function (loc) { byName[loc.name] = loc; });

  if (routeNames && routeNames.length > 1) {
    let d = "";
    routeNames.forEach(function (name, i) {
      const loc = byName[name];
      if (!loc || !NODE_LAYOUT[loc.id]) return;
      const p = NODE_LAYOUT[loc.id];
      d += (i === 0 ? "M" : "L") + p[0] + " " + p[1] + " ";
    });
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", d);
    path.setAttribute("class", "path-line");
    svg.appendChild(path);
  }

  locations.forEach(function (loc) {
    const p = NODE_LAYOUT[loc.id] || [50, 50];
    const g = document.createElementNS("http://www.w3.org/2000/svg", "g");
    const c = document.createElementNS("http://www.w3.org/2000/svg", "circle");
    c.setAttribute("cx", p[0]);
    c.setAttribute("cy", p[1]);
    c.setAttribute("r", loc.isHospital ? 11 : 8);
    let cls = loc.isHospital ? "node-hospital" : "node-core";
    if (routeNames && routeNames.indexOf(loc.name) !== -1) cls = loc.isHospital ? "node-hospital" : "node-active";
    c.setAttribute("class", cls);
    const t = document.createElementNS("http://www.w3.org/2000/svg", "text");
    t.setAttribute("x", p[0] + 12);
    t.setAttribute("y", p[1] + 4);
    t.setAttribute("class", "node-label");
    t.textContent = loc.id + " " + loc.name.replace(/_/g, " ");
    g.appendChild(c);
    g.appendChild(t);
    svg.appendChild(g);
  });
}

function renderOverview(status, beds) {
  $("stat-occupied").textContent = beds.occupied;
  $("stat-capacity").textContent = "/ " + beds.maxBeds;
  $("stat-available").textContent = beds.available;
  $("stat-queue").textContent = status.queueSize;
  if (status.nextInQueue) {
    $("stat-next").textContent = status.nextInQueue.name;
    $("stat-next-pri").textContent = status.nextInQueue.priorityLabel;
  } else {
    $("stat-next").textContent = "None";
    $("stat-next-pri").textContent = "";
  }
  const pct = beds.maxBeds ? Math.round((beds.occupied / beds.maxBeds) * 100) : 0;
  $("occupancy-bar").style.width = pct + "%";
  $("occupancy-label").textContent = beds.occupied + " of " + beds.maxBeds + " beds occupied (" + pct + "%)";
}

function renderWards(status) {
  const body = $("ward-body");
  body.innerHTML = "";
  if (!status.patients || status.patients.length === 0) {
    body.innerHTML = "<tr><td colspan='5'>All beds are currently vacant.</td></tr>";
    return;
  }
  status.patients.forEach(function (p) {
    const tr = document.createElement("tr");
    tr.innerHTML = "<td>" + p.bed + "</td><td>" + p.id + "</td><td>" + p.name +
      "</td><td>" + p.ward + "</td><td>" + p.ambulanceFare + "</td>";
    body.appendChild(tr);
  });

  const dis = $("dis-id");
  const current = dis.value;
  dis.innerHTML = "";
  status.patients.forEach(function (p) {
    const opt = document.createElement("option");
    opt.value = p.id;
    opt.textContent = p.id + " — " + p.name;
    dis.appendChild(opt);
  });
  if (current) dis.value = current;
}

function renderPharmacy(data) {
  const wrap = $("pharmacy-cards");
  wrap.innerHTML = "";
  (data.medicines || []).forEach(function (m) {
    const el = document.createElement("article");
    el.className = "card";
    el.innerHTML = "<span>Medicine</span><strong>" + m.name + "</strong><p>Stock: " +
      m.stock + " units</p><p>Price: Rs. " + m.price + "</p>";
    wrap.appendChild(el);
  });
}

function renderDispatchBanner(status) {
  const el = $("dispatch-next");
  if (!status.nextInQueue) {
    el.textContent = "No pending emergency vehicle calls.";
  } else {
    el.innerHTML = "Next in queue: <strong>" + status.nextInQueue.name +
      "</strong> <span class='pill " + (status.nextInQueue.priority === 1 ? "critical" : "normal") +
      "'>" + status.nextInQueue.priorityLabel + "</span>";
  }
}

async function refreshAll() {
  const [locs, status, beds, pharmacy] = await Promise.all([
    api("/api/locations"),
    api("/api/status"),
    api("/api/beds"),
    api("/api/pharmacy")
  ]);
  locations = locs.locations || [];
  fillLocationSelects();
  drawMap(lastRoute);
  renderOverview(status, beds);
  renderWards(status);
  renderPharmacy(pharmacy);
  renderDispatchBanner(status);
}

function switchView(name) {
  Object.keys(views).forEach(function (key) {
    $("view-" + key).classList.toggle("hidden", key !== name);
  });
  document.querySelectorAll(".nav-btn").forEach(function (btn) {
    btn.classList.toggle("active", btn.getAttribute("data-view") === name);
  });
  $("page-title").textContent = views[name][0];
  $("page-sub").textContent = views[name][1];
}

document.querySelectorAll(".nav-btn").forEach(function (btn) {
  btn.addEventListener("click", function () { switchView(btn.getAttribute("data-view")); });
});

$("refresh-btn").addEventListener("click", function () {
  refreshAll().then(function () { showAlert("Live data refreshed from C++ backend.", true); })
    .catch(function (err) { showAlert(err.message, false); });
});

$("route-form").addEventListener("submit", function (e) {
  e.preventDefault();
  const from = $("route-from").value;
  api("/api/route?from=" + encodeURIComponent(from)).then(function (data) {
    lastRoute = data.route || [];
    drawMap(lastRoute);
    $("route-result").textContent =
      data.fromName + " → " + data.toName + "\n" +
      "Distance: " + data.distanceKm + " km\n" +
      "ETA: " + data.estimatedTimeMins + " mins @ 40 km/h\n" +
      "Path: " + (data.route || []).join(" → ");
    showAlert("Dijkstra route computed by CityMap.", true);
  }).catch(function (err) {
    showAlert(err.message, false);
  });
});

$("emergency-form").addEventListener("submit", function (e) {
  e.preventDefault();
  const payload = {
    pickupLoc: parseInt($("em-pickup").value, 10),
    id: parseInt($("em-id").value, 10),
    name: $("em-name").value.trim(),
    priority: parseInt($("em-priority").value, 10)
  };
  fetch("/api/emergency", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  }).then(function (res) { return res.json().then(function (data) { return { res: res, data: data }; }); })
    .then(function (pack) {
      if (!pack.res.ok || pack.data.ok === false) throw new Error(pack.data.error || "Request failed");
      $("emergency-result").textContent = pack.data.message;
      showAlert("Emergency queued in the C++ priority queue.", true);
      return refreshAll();
    }).catch(function (err) { showAlert(err.message, false); });
});

$("dispatch-btn").addEventListener("click", function () {
  fetch("/api/dispatch", { method: "POST" })
    .then(function (res) { return res.json().then(function (data) { return { res: res, data: data }; }); })
    .then(function (pack) {
      if (!pack.res.ok || pack.data.ok === false) throw new Error(pack.data.error || "Dispatch failed");
      const data = pack.data;
      if (data.empty) {
        $("dispatch-result").textContent = data.message;
      } else {
        lastRoute = data.route || [];
        drawMap(lastRoute);
        $("dispatch-result").textContent =
          "Patient: " + data.name + " (ID " + data.patientId + ")\n" +
          "Condition: " + data.condition + "\n" +
          "Pickup: " + data.pickupPoint + "\n" +
          "Route: " + (data.route || []).join(" → ") + "\n" +
          "Distance: " + data.distanceKm + " km\n" +
          "ETA: " + data.estimatedTimeMins + " mins\n" +
          "Fare: Rs. " + data.ambulanceFare + "\n" +
          "Bed: " + data.bedAssigned + " / " + data.maxBeds + " (" + data.ward + ")";
      }
      showAlert(data.empty ? data.message : "Dispatch completed by HospitalSystem.", !data.empty);
      return refreshAll();
    }).catch(function (err) { showAlert(err.message, false); });
});

$("discharge-form").addEventListener("submit", function (e) {
  e.preventDefault();
  const payload = {
    patientId: parseInt($("dis-id").value, 10),
    daysStayed: parseInt($("dis-days").value, 10)
  };
  fetch("/api/discharge", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  }).then(function (res) { return res.json().then(function (data) { return { res: res, data: data }; }); })
    .then(function (pack) {
      if (!pack.res.ok || pack.data.ok === false) throw new Error(pack.data.error || "Discharge failed");
      const inv = pack.data;
      $("invoice-panel").innerHTML =
        "<h3>Final invoice</h3>" +
        "<p><strong>" + inv.name + "</strong> · ID " + inv.patientId + "</p>" +
        "<p>" + inv.ward + "</p>" +
        "<p>Stay: " + inv.daysStayed + " day(s)</p>" +
        "<p>Ambulance: Rs. " + inv.ambulanceFare + "</p>" +
        "<p>Room rent: Rs. " + inv.roomRent + "</p>" +
        "<p><strong>Total: Rs. " + inv.totalBill + "</strong></p>" +
        "<p class='muted'>Saved to " + (inv.receiptFile || "receipt file") + "</p>";
      showAlert("Patient discharged. Receipt written by C++.", true);
      return refreshAll();
    }).catch(function (err) { showAlert(err.message, false); });
});

refreshAll().catch(function (err) { showAlert(err.message, false); });
