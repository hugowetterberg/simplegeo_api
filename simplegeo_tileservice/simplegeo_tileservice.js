(function(SimpleGeoMap){
  var layers = [], enabledLayers = [],
  source = {
    title: Drupal.t('Layers'),
    init: function() {
      var name, layerCookie = $.cookie('simplegeoMapTileserviceLayers');
      layers = Drupal.settings.simpleGeoMap.tileservice.layers;

      // Set initial enabled filters based on Drupal.setting, cookie or filter setting,
      if (Drupal.settings.simpleGeoMap.tileservice.enabledLayers) {
        enabledLayers = Drupal.settings.simpleGeoMap.tileservice.enabledLayers;
      }
      else if (layerCookie) {
        enabledLayers = layerCookie.split(',');
      }
      else {
        for (name in layers) {
          enabledLayers.push(name);
        }
      }

      return $('<ul id="simplegeomap-tileservice" class="dialog"><div class="wrapper"></div></ul>');
    },
    query: function(data) {
      console.log(enabledLayers);
      data.layers = enabledLayers.join(',');
      return Drupal.settings.basePath + 'geo/api/layers';
    },
    result: function(json, pushMarker) {
      $.each(enabledLayers, function (j, type) {
        var i, u;
        if (typeof json[type] !== 'undefined') {
          for (i=0; i<json[type].length; i++) {
            for (u=0; u<json[type][i].length; u++) {
              pushMarker(type, json[type][i][u]);
            }
          }
        }
      });
    }
  };
  SimpleGeoMap.addSource('tileservice', source);
})(SimpleGeoMap);


