(function(SimpleGeoMap){
  var layers, enabledLayers = {},
  source = {
    title: Drupal.t('Layers'),
    init: function() {
      var name, dialog, defaultLayers,
      layerCookie = $.cookie('simplegeoMapTileserviceLayers'),
      layers = Drupal.settings.simpleGeoMap.tileservice.layers;

      // Set initial enabled filters based on Drupal.setting, cookie or filter setting,
      if (Drupal.settings.simpleGeoMap.tileservice.enabledLayers) {
        defaultLayers = Drupal.settings.simpleGeoMap.tileservice.enabledLayers;
      }
      else if (layerCookie) {
        defaultLayers = layerCookie.split(',');
      }
      else {
        defaultLayers = layers;
      }
      for (name in defaultLayers) {
        enabledLayers[name] = layers[name];
      }

      dialog = $('<ul id="simplegeomap-tileservice" class="dialog"></ul>');
      $.each(layers, function(name, title) {
        var item = $('<li><a href="#"></a></li>').appendTo(dialog),
        a = $('a', item).text(title).click(function() {
          if (enabledLayers[name]) {
            a.removeClass('active');
            delete enabledLayers[name];
          }
          else {
            a.addClass('active');
            enabledLayers[name] = title;
          }
          SimpleGeoMap.updateMarkers(true);
        });
        if (enabledLayers[name]) {
          a.addClass('active');
        }
        item.addClass('layer-' + name);
      });
      return dialog;
    },
    query: function(data) {
      var url = null, name, names = [];
      for (name in enabledLayers) {
        names.push(name);
      }
      if (names.length) {
        data.layers = names.join(',');
        url = Drupal.settings.basePath + 'geo/api/layers';
      }
      return url;
    },
    result: function(json, pushMarker) {
      $.each(enabledLayers, function (name, title) {
        var i, u;
        if (typeof json[name] !== 'undefined') {
          for (i=0; i<json[name].length; i++) {
            for (u=0; u<json[name][i].length; u++) {
              pushMarker(name, json[name][i][u]);
            }
          }
        }
      });
    }
  };
  SimpleGeoMap.addSource('tileservice', source);
})(SimpleGeoMap);


