var clock = new THREE.Clock();
var scene = new THREE.Scene();

var camera = new THREE.PerspectiveCamera(30, window.innerWidth/window.innerHeight, 0.1, 1000); 
camera.position.z = 200;

var renderer = new THREE.WebGLRenderer();
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setClearColor(0xffffff, 1);
document.body.appendChild(renderer.domElement);

var pointLight = new THREE.PointLight(0xffffff);
pointLight.position.x = 100;
pointLight.position.y = 100;
pointLight.position.z = 100;
scene.add(pointLight);

var directionalLight = new THREE.DirectionalLight(0xffffff);
scene.add(directionalLight);

cameraControls = new THREE.TrackballControls(camera, renderer.domElement);
cameraControls.target.set(0, 0, 0);
cameraControls.zoomSpeed = 0.04;
cameraControls.panSpeed = 0.8;
// cameraControls.addEventListener("change", render); // not working.. sigh

var vortexId = [];
var vortexIdPos3D = [];
var displayVortexId = true;

window.addEventListener("resize", onResize, false);

function render() {
  // scene
  var delta = clock.getDelta();
  requestAnimationFrame(render);
  cameraControls.update(delta);
  directionalLight.position.copy(camera.position);
  renderer.render(scene, camera);

  renderVortexId();
}

function onResize() {
  camera.aspect = window.innerWidth/window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
  cameraControls.handleResize();
}

function renderVortexId () {
  for (i=0; i<vortexIdPos3D.length; i++) {
    var vector = vortexIdPos3D[i].clone();
    vector.project(camera);
    vector.x = (vector.x + 1)/2 * window.innerWidth;
    vector.y = -(vector.y - 1)/2 * window.innerHeight;

    var text2 = document.getElementById("vortexId"+i);
    if (text2 == null) {
      text2 = document.createElement("div");
      text2.id = "vortexId" + i;
      text2.style.position = "absolute";
      document.body.appendChild(text2);
    }

    if (vector.x >= 0 && vector.x < window.innerWidth-50 && 
        vector.y >= 0 && vector.y < window.innerHeight-25) {
      text2.style.top = vector.y;
      text2.style.left = vector.x;
      text2.innerHTML = vortexId[i];
      text2.style.display = "block";
    } else {
      text2.style.display = "none";
    }

    if (!displayVortexId)
      text2.style.display = "none";
  }
}