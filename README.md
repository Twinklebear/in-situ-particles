To run use a script to query some data from the simulation

```js
var bounds = box3f();
var iss = pkdPollOnce(r, "localhost", 29374, 0.015, bounds);
m.addGeometry(iss);
m.commit();

setWorldBounds(bounds);
print(bounds);
refresh();
```

