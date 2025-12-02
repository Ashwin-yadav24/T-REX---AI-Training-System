// Smooth scrolling for navigation links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            target.scrollIntoView({
                behavior: 'smooth',
                block: 'start'
            });
        }
    });
});

// Navbar scroll effect
let lastScrollTop = 0;
const navbar = document.querySelector('.navbar');

window.addEventListener('scroll', () => {
    let scrollTop = window.pageYOffset || document.documentElement.scrollTop;
    
    if (scrollTop > lastScrollTop) {
        navbar.style.transform = 'translateY(-100%)';
    } else {
        navbar.style.transform = 'translateY(0)';
    }
    
    lastScrollTop = scrollTop;
});

// Animated counters
function animateCounters() {
    const counters = document.querySelectorAll('.stat-number');
    const speed = 200;

    counters.forEach(counter => {
        const updateCount = () => {
            // Determine target based on content, or default to a reasonable number
            const target = parseInt(counter.getAttribute('data-target')) || 
                          (counter.textContent.includes('K') ? 15 : 
                           counter.textContent.includes('x') ? 10 : 360);
            
            const count = parseInt(counter.innerText) || 0;
            const inc = target / speed;

            if (count < target) {
                counter.innerText = Math.ceil(count + inc);
                setTimeout(updateCount, 1);
            } else {
                // Keep the suffix (K, x, °) if it was present
                if (counter.textContent.includes('K')) {
                    counter.innerText = target + 'K';
                } else if (counter.textContent.includes('x')) {
                    counter.innerText = target + 'x';
                } else if (counter.textContent.includes('°')) {
                    counter.innerText = target + '°';
                } else {
                    counter.innerText = target;
                }
            }
        };
        updateCount();
    });
}

// Intersection Observer for animations
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -50px 0px'
};

const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.classList.add('animate');
            
            // Trigger counter animation for stats
            if (entry.target.classList.contains('hero-stats')) {
                animateCounters();
            }
        }
    });
}, observerOptions);

// Observe elements for animation
document.querySelectorAll('.feature-card, .pricing-card, .perfect-card, .hero-stats, .product-image-container, .three-d-container').forEach(el => {
    observer.observe(el);
});

// Button interactions
document.querySelectorAll('.btn-primary, .btn-secondary, .get-started-btn').forEach(btn => {
    btn.addEventListener('click', function(e) {
        // Create ripple effect
        const ripple = document.createElement('span');
        ripple.classList.add('ripple');
        this.appendChild(ripple);
        
        const rect = this.getBoundingClientRect();
        const size = Math.max(rect.width, rect.height);
        const x = e.clientX - rect.left - size / 2;
        const y = e.clientY - rect.top - size / 2;
        
        ripple.style.width = ripple.style.height = size + 'px';
        ripple.style.left = x + 'px';
        ripple.style.top = y + 'px';
        
        setTimeout(() => {
            ripple.remove();
        }, 600);
    });
});

// Add ripple effect CSS
const style = document.createElement('style');
style.textContent = `
    .ripple {
        position: absolute;
        border-radius: 50%;
        background: rgba(255, 255, 255, 0.3);
        pointer-events: none;
        animation: ripple-animation 0.6s linear;
    }
    
    @keyframes ripple-animation {
        to {
            transform: scale(2);
            opacity: 0;
        }
    }
    
    .animate {
        animation: fadeInUp 0.6s ease-out;
    }
    
    @keyframes fadeInUp {
        from {
            opacity: 0;
            transform: translateY(30px);
        }
        to {
            opacity: 1;
            transform: translateY(0);
        }
    }
`;
document.head.appendChild(style);

// Device status updates (simulation)
function updateDeviceStatus() {
    const statusElements = {
        reactionTime: document.querySelector('.status-row:nth-child(3) .status'),
        accuracy: document.querySelector('.status-row:nth-child(4) .status')
    };
    
    if (statusElements.reactionTime) {
        const times = ['0.24s', '0.21s', '0.19s', '0.22s', '0.18s'];
        const randomTime = times[Math.floor(Math.random() * times.length)];
        statusElements.reactionTime.textContent = randomTime;
    }
    
    if (statusElements.accuracy) {
        const accuracies = ['94.2%', '96.1%', '92.8%', '95.7%', '93.4%'];
        const randomAccuracy = accuracies[Math.floor(Math.random() * accuracies.length)];
        statusElements.accuracy.textContent = randomAccuracy;
    }
}

// Update device status every 3 seconds
setInterval(updateDeviceStatus, 3000);

// --- 3D Model Logic (Three.js) ---
function initThreeDModel() {
    // Check if Three.js is loaded
    if (typeof THREE === 'undefined') {
        console.error("Three.js not loaded. Cannot initialize 3D model.");
        return;
    }

    const canvas = document.getElementById('threeDCanvas');
    if (!canvas) return;

    const container = canvas.parentElement;
    
    // 1. Scene setup
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x000000); // Black background to match the theme
    
    // 2. Camera setup
    const camera = new THREE.PerspectiveCamera(75, container.clientWidth / container.clientHeight, 0.1, 1000);
    camera.position.z = 3;
    
    // 3. Renderer setup
    const renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: true });
    renderer.setSize(container.clientWidth, container.clientHeight);

    // Handle resizing
    const onWindowResize = () => {
        // Ensure the canvas size adapts to the container's current size
        const width = container.clientWidth;
        const height = container.clientHeight;
        
        renderer.setSize(width, height);
        camera.aspect = width / height;
        camera.updateProjectionMatrix();
    };
    window.addEventListener('resize', onWindowResize);
    onWindowResize(); // Initial size setup

    // 4. Geometry (Placeholder Model - A stylized device cube with a laser)
    const geometry = new THREE.BoxGeometry(1.5, 1.5, 1.5);
    
    // Material (Stylized to match the T-REX dark/neon theme)
    const material = new THREE.MeshPhongMaterial({
        color: 0x1a1a1a,
        shininess: 100,
        specular: 0xaaaaaa,
        transparent: true,
        opacity: 0.9
    });
    
    // Create the main device model
    const model = new THREE.Mesh(geometry, material);
    scene.add(model);

    // Add a glowing laser-like element (Cylinder)
    const laserGeometry = new THREE.CylinderGeometry(0.05, 0.05, 3, 32);
    const laserMaterial = new THREE.MeshBasicMaterial({ color: 0xff4444 });
    const laser = new THREE.Mesh(laserGeometry, laserMaterial);
    laser.rotation.z = Math.PI / 2; // Pointing to the side
    laser.position.x = 0;
    laser.position.y = 1;
    scene.add(laser);
    
    // Add point light source for glow effect
    const pointLight = new THREE.PointLight(0xff4444, 5, 50);
    pointLight.position.set(0, 1, 0);
    scene.add(pointLight);

    // 5. Lighting
    const ambientLight = new THREE.AmbientLight(0x404040, 5); // soft white light
    scene.add(ambientLight);
    
    const directionalLight = new THREE.DirectionalLight(0xffffff, 1);
    directionalLight.position.set(5, 5, 5);
    scene.add(directionalLight);

    // 6. Interaction Variables
    let isDragging = false;
    let previousMousePosition = {
        x: 0,
        y: 0
    };

    // 7. Mouse/Touch Interaction
    function onMouseDown(event) {
        isDragging = true;
    }

    function onMouseUp(event) {
        isDragging = false;
    }

    function onMouseMove(event) {
        if (!isDragging) return;

        // Calculate deltas for rotation
        const deltaX = event.clientX - previousMousePosition.x;
        const deltaY = event.clientY - previousMousePosition.y;

        // Rotation speed control
        model.rotation.y += deltaX * 0.005;
        model.rotation.x += deltaY * 0.005;

        previousMousePosition = {
            x: event.clientX,
            y: event.clientY
        };
    }

    // Touch events for mobile
    function onTouchStart(event) {
        if (event.touches.length === 1) {
            previousMousePosition.x = event.touches[0].clientX;
            previousMousePosition.y = event.touches[0].clientY;
            isDragging = true;
        }
        event.preventDefault(); // Prevent scrolling while rotating
    }

    function onTouchEnd(event) {
        isDragging = false;
    }

    function onTouchMove(event) {
        if (!isDragging || event.touches.length !== 1) return;

        const touch = event.touches[0];
        const deltaX = touch.clientX - previousMousePosition.x;
        const deltaY = touch.clientY - previousMousePosition.y;

        // Rotation speed control
        model.rotation.y += deltaX * 0.005;
        model.rotation.x += deltaY * 0.005;

        previousMousePosition = {
            x: touch.clientX,
            y: touch.clientY
        };
        event.preventDefault(); // Prevent scrolling while rotating
    }


    canvas.addEventListener('mousedown', onMouseDown);
    window.addEventListener('mouseup', onMouseUp);
    window.addEventListener('mousemove', onMouseMove);
    
    canvas.addEventListener('touchstart', onTouchStart);
    canvas.addEventListener('touchend', onTouchEnd);
    canvas.addEventListener('touchmove', onTouchMove);

    // 8. Animation loop
    const animate = () => {
        requestAnimationFrame(animate);

        // Continuous slow rotation if not dragging
        if (!isDragging) {
             model.rotation.y += 0.001;
        }
        
        // Simple laser pulse animation
        const time = Date.now() * 0.001;
        const scale = 1 + Math.sin(time * 3) * 0.1;
        laser.scale.set(1, 1, scale);
        
        renderer.render(scene, camera);
    };
    
    // Start the loop
    animate();
}
// --- End 3D Model Logic ---


// Initialize
document.addEventListener('DOMContentLoaded', function() {
    // Add loading animation
    document.body.classList.add('loaded');
    
    // Start device status updates
    updateDeviceStatus();

    // Initialize 3D Model
    initThreeDModel();
});
