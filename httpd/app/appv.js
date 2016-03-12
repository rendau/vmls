define([
    'jquery', 'Backbone', 'app', 'authv', 'tulip/list', 'tulip/sptr', 'camsv', 'stv'
], function($ , BB, app, authv, List, Sptr, camsv, stv) {

    var V = BB.View.extend({
        tagName: 'div',
        className: 'app',
        initialize: function(opts) {
	    var hs = new Sptr({ type: 'h' });
	    this.ml = new List({
		idAttr: 'mid',
		itemSelectH: this.menuSelectH.bind(this),
	    });
	    var ms = new Sptr({
		type: 'vr',
		$target: $('<div>').addClass('menu').append(this.ml.$dn),
	    });
	    this.$body = $('<div>').addClass('body');
	    this.$el.append(
		$('<div>').addClass('app_hd').append(
		    $('<div>').addClass('title').html('Video Monitoring')
		),
		// hs.$dn,
		$('<div>').addClass('app_scWrap').append(
		    $('<div>').addClass('app_sc').append(
			ms.$target,
			ms.$dn,
			this.$body
		    )
		)
	    ).hide();
	    this.menus = [ camsv, stv ];

	    app.on('change:authorized', this.authorized_h, this);
        },
	authorized_h: function(m, v) {
	    if(v) {
		this.$el.show();
		this.ml.refreshItems(this.menus);
	    } else {
		this.$el.hide();
		this.ml.clearItems();
	    }
	},
	menuSelectH: function(m) {
	    if(this.sm) {
		this.$body.children().detach();
		this.sm.onHide && this.sm.onHide();
	    }
	    this.sm = m;
	    if(this.sm) {
		this.$body.append(this.sm.$el);
		this.sm.onShow && this.sm.onShow();
	    }
	},
    });

    return V;

});
