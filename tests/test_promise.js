var passes = [];

var p = new Promise(function(res,rej) {
  setTimeout(res, 10, "Hello"); 
}).then(function(r) {  
  if (r=="Hello") passes.push("SimpleResolve");
});

// Check that promises work even if the function is called immediately
var p = new Promise(function(res,rej) {
  rej("Hello");
}).catch(function(r) {  
  if (r=="Hello") passes.push("SimpleReject");
}).then(function(r) {  
  passes.push("SimpleReject2");
});

var p = new Promise(function(res,rej) {
  res("Hello"); 
}).then(function(r) {  
  if (r=="Hello") passes.push("InstantResolve");
}).catch(function(r) {  
  passes.push("FAIL1");
});

var p = new Promise(function(res,rej) {
  setTimeout(res, 10, "Hello"); 
}).then(function(r) {  
  if (r=="Hello") passes.push("Resolve1");
}).then(function(r) {  
  if (r=="Hello") passes.push("Resolve2");
});


var p = Promise.resolve("Hello").then(function(r) {  
  if (r=="Hello") passes.push("PreResolved");
}).catch(function(r) {  
  passes.push("FAIL2");
});

var p = Promise.reject("Hello").catch(function(r) {  
  if (r=="Hello") passes.push("PreRejected");
}).then(function(r) {  
  passes.push("PreRejected2");
});


var p = Promise.all([new Promise(function(res,rej) {
  setTimeout(res, 10, "A"); 
}), new Promise(function(res,rej) {
  setTimeout(res, 10, "B"); 
})]).then(function(r) {  
  if (r=="A,B") passes.push("ResolveAll");
});

var p = Promise.all([new Promise(function(res,rej) {
  setTimeout(res, 20, "A"); 
}), new Promise(function(res,rej) {
  setTimeout(rej, 10, "Ok"); 
})]).then(function(r) {  
  passes.push("FAIL5");
}).catch(function(r) {  
  if (r=="Ok") passes.push("RejectAll");
});

Promise.all([]).then(function(r) {  
  passes.push("ResolveAll[]"); //  https://github.com/espruino/Espruino/issues/2371
});

var resolved = Promise.resolve("test");
setTimeout(function() {
  Promise.all([42]).then(function(r) {  
    passes.push("ResolveAll[42]"); //  https://github.com/espruino/Espruino/issues/2371
  });
  Promise.all([resolved]).then(function(r) {  
    if (r=="test")
      passes.push("ResolveAll[resolved]"); //  https://github.com/espruino/Espruino/issues/2371
  });  
}, 1);


setTimeout(function() {
  passes.sort();
  result = passes == "InstantResolve,PreRejected,PreRejected2,PreResolved,RejectAll,Resolve1,ResolveAll,ResolveAll[42],ResolveAll[],ResolveAll[resolved],SimpleReject,SimpleReject2,SimpleResolve";
  if (!result) console.log(""+passes);
},30);
