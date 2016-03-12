define([
    'jquery', 'Backbone', 'cams', 'tulip/btn', 'tulip/grid',
    'tulip/lbl', 'tulip/txt', 'tulip/txta'
], function($, BB, cams, Btn, Grid, Lbl, Txt, Txta) {
    var V = BB.View.extend({
        className: 'cams',
        mid: 'cams',
        name: 'Камеры',
        initialize: function() {
	    this.nbt = new Btn({
		labelHtml: 'Создать',
		click_h: this.nbt_click_h.bind(this),
	    });
	    this.ebt = new Btn({
		labelHtml: 'Изменить',
		click_h: this.ebt_click_h.bind(this),
		disabled: true,
	    });
	    this.dbt = new Btn({
		labelHtml: 'Удалить',
		click_h: this.dbt_click_h.bind(this),
		disabled: true,
	    });
	    this.grid = new Grid({
		// showHeader: false,
		model: [
		    // {id: 'id', text: 'ID', w: '80px'},
		    {id: 'working', text: '', w: '50px', tf: this.grid_wst_ctf},
		    {id: 'addr', text: 'Адрес', w: '200px', tf: this.grid_addr_ctf},
		    {id: 'name', text: 'Наименование', w: '270px'},
		    {id: 'dsc', text: 'Описание'},
		],
		rowSelectH: this.rowSelectH.bind(this),
	    });
	    this.$list_v = $('<div>').addClass('cams_list').append(
		$('<div>').addClass('tbar').append(
		    this.nbt.$dn, '&nbsp;', this.ebt.$dn, '&nbsp;', this.dbt.$dn
		),
		$('<div>').addClass('bodyWrap').append(
		    $('<div>').append(this.grid.$dn)
		)
	    );
	    this.f_$hdr = $('<td>').addClass('hdr');
	    var l1 = new Lbl({html: 'Ссылка:'});
	    this.f_url = new Txt({width: '500px'});
	    var l2 = new Lbl({html: 'Наименование:'});
	    this.f_name = new Txt({width: '220px'});
	    var l3 = new Lbl({html: 'Описание:'});
	    this.f_dsc = new Txta({width: '350px', height: '5rem'});
	    this.sbt = new Btn({
		labelHtml: 'Сохранить',
		click_h: this.sbt_click_h.bind(this),
	    });
	    this.cbt = new Btn({
		labelHtml: 'Отмена',
		click_h: this.cbt_click_h.bind(this),
	    });
	    this.$form_v = $('<div>').addClass('cams_form').append(
		$('<table>').append(
		    $('<tr>').append(
			this.f_$hdr.attr('colspan', '2')
		    ),
		    $('<tr>').append(
			$('<td>').append(l1.$dn),
			$('<td>').append(this.f_url.$dn)
		    ),
		    $('<tr>').append(
			$('<td>').append(l2.$dn),
			$('<td>').append(this.f_name.$dn)
		    ),
		    $('<tr>').append(
			$('<td>').addClass('vaTop').append(l3.$dn),
			$('<td>').append(this.f_dsc.$dn)
		    ),
		    $('<tr>').append(
			$('<td>').attr('colspan', '2').addClass('act').append(
			    this.sbt.$dn, '&nbsp;', this.cbt.$dn
			)
		    )
		)
	    );

	    cams.on({
		'add': this.m_add_h,
		'change': this.m_change_h,
		'remove': this.m_remove_h,
	    }, this);
        },
	showList: function() {
	    this.$el.children().detach();
	    this.$el.append(this.$list_v);
	},
	showForm: function(id) {
	    var m = (id ? cams.get(id) : null);
	    this.$el.children().detach();
	    this.$el.append(this.$form_v);
	    this.f_$hdr.text((m ? 'Изменение камеры': 'Новая камера'));
	    this._setFormVals((m ? m.attributes : {url: 'rtsp://localhost/asd/dsa'}));
	    this.f_name.focus();
	    this.sbt.enable();
	    this.formModel = m;
	},
	_setFormVals: function(o) {
	    this.f_url.val(o.url || '');
	    this.f_name.val(o.name || '');
	    this.f_dsc.val(o.dsc || '');
	},
	_getFormVals: function() {
	    return {
		url: this.f_url.val(),
		name: this.f_name.val(),
		dsc: this.f_dsc.val()
	    };
	},
	onShow: function() {
	    this.showList();
	},
	m_add_h: function(m, c, o) {
	    this.grid.addRow(m.attributes);
	},
	m_change_h: function(m, o) {
	    this.grid.updateRow(m.id, m.attributes);
	},
	m_remove_h: function(m, o) {
	    this.grid.removeRow(m.id);
	},
	rowSelectH: function(row) {
	    this.ebt.setDisabled(!row);
	    this.dbt.setDisabled(!row);
	},
	nbt_click_h: function() {
	    this.showForm();
	},
	ebt_click_h: function() {
	    var sel = this.grid.selected();
	    sel && this.showForm(sel.id);
	},
	dbt_click_h: function() {
	    var self = this;
	    var sel = self.grid.selected();
	    var m = (sel ? cams.get(sel.id) : null);
	    m && m.destroy({ wait: true });
	},
	sbt_click_h: function() {
	    var self = this;
	    var _error = function(m, err) {
		self.sbt.enable();
		err || self.showList();
	    };
	    var fv = self._getFormVals(), m = self.formModel;
	    self.sbt.disable();
	    if(m) {
		m.save(fv, { wait: true, error: _error });
		m.validationError && _error(m, true);
	    } else {
		m = cams.create(fv, { wait: true, error: _error });
		m.validationError && _error(m, true);
	    }
	},
	cbt_click_h: function() {
	    this.showList();
	},
	grid_addr_ctf: function(row, v) {
	    var h = row.url.match(':\/\/[^/]+');
	    return (h ? h[0].slice(3) : '');
	},
	grid_wst_ctf: function(row, v) {
	    return $('<div>').addClass('cams_wst_'+(v ? 't' : 'f'));
	}
    });

    return (new V);
});
