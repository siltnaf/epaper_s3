var store = {
  currentPage: 'home',
  pages: [
    { id: 'home', name: '首页', icon: '🏠' },
    { id: 'library', name: '书库', icon: '📖' },
    { id: 'voice', name: '有声书', icon: '🎧' },
    { id: 'songs', name: '儿歌', icon: '🎵' },
    { id: 'poems', name: '古詩', icon: '📜' },
    { id: 'imglearn', name: '圖像識字', icon: '🎨' },
    { id: 'clock', name: '时钟', icon: '🕐' },
    { id: 'games', name: '游戏', icon: '🎮' },
  ],
  books: [],
  bookTotal: 0,
  libPage: 1,
  libPerPage: 10,
  voiceStories: [],
  voicePage: 1,
  voicePerPage: 10,
  voiceTotal: 0,
  kidSongs: [],
  songPage: 1,
  songPerPage: 10,
  songTotal: 0,
  playingSong: null,
  songStatus: '停止',
  songAudio: null,
  poems: [],
  poemPage: 1,
  poemPerPage: 10,
  poemTotal: 0,
  imageLearnItems: [],
  imgLearnPage: 1,
  imgLearnPerPage: 6,
  imgLearnTotal: 0,
  imageLearnLoading: false,
  imageLearnError: '',
  selectedBook: null,
  playingStory: null,
  playingPoem: null,
  currentGame: null,
  speakingStatus: '停止',
  loading: false,
  error: '',

  // --- Clock & Weather ---
  clockCity: '',
  clockTimezone: '',
  clockTime: '',
  clockDate: '',
  locationMessage: '',
  weatherData: null,
  weatherLoading: false,
  weatherError: '',
  _clockAnimFrame: null,

  get pagedBooks() {
    return this.books
  },
  get libTotalPages() {
    return Math.ceil(this.bookTotal / this.libPerPage) || 1
  },
  libPrevPage() {
    if (this.libPage > 1) {
      this.libPage--
      this.loadBooks(this.libPage)
      window.scrollTo(0, 0)
    }
  },
  libNextPage() {
    if (this.libPage < this.libTotalPages) {
      this.libPage++
      this.loadBooks(this.libPage)
      window.scrollTo(0, 0)
    }
  },

  /* paginated voice stories - server-side */
  get pagedVoiceStories() {
    return this.voiceStories
  },
  get voiceTotalPages() {
    return Math.ceil(this.voiceTotal / this.voicePerPage) || 1
  },
  voicePrevPage() {
    if (this.voicePage > 1) {
      this.voicePage--
      this.loadVoiceStories(this.voicePage)
      window.scrollTo(0, 0)
    }
  },
  voiceNextPage() {
    if (this.voicePage < this.voiceTotalPages) {
      this.voicePage++
      this.loadVoiceStories(this.voicePage)
      window.scrollTo(0, 0)
    }
  },

  /* paginated kid songs - server-side */
  get pagedKidSongs() {
    return this.kidSongs
  },
  get songTotalPages() {
    return Math.ceil(this.songTotal / this.songPerPage) || 1
  },
  songPrevPage() {
    if (this.songPage > 1) {
      this.songPage--
      this.loadKidSongs(this.songPage)
      window.scrollTo(0, 0)
    }
  },
  songNextPage() {
    if (this.songPage < this.songTotalPages) {
      this.songPage++
      this.loadKidSongs(this.songPage)
      window.scrollTo(0, 0)
    }
  },

  /* paginated poems - server-side */
  get pagedPoems() {
    return this.poems
  },
  get poemTotalPages() {
    return Math.ceil(this.poemTotal / this.poemPerPage) || 1
  },
  poemPrevPage() {
    if (this.poemPage > 1) {
      this.poemPage--
      this.loadPoems(this.poemPage)
      window.scrollTo(0, 0)
    }
  },
  poemNextPage() {
    if (this.poemPage < this.poemTotalPages) {
      this.poemPage++
      this.loadPoems(this.poemPage)
      window.scrollTo(0, 0)
    }
  },

  /* paginated image learn - server-side */
  get pagedImageLearn() {
    return this.imageLearnItems
  },
  get imgLearnTotalPages() {
    return Math.ceil(this.imgLearnTotal / this.imgLearnPerPage) || 1
  },
  imgLearnPrevPage() {
    if (this.imgLearnPage > 1) {
      this.imgLearnPage--
      this.loadImageLearnItems(this.imgLearnPage)
      window.scrollTo(0, 0)
    }
  },
  imgLearnNextPage() {
    if (this.imgLearnPage < this.imgLearnTotalPages) {
      this.imgLearnPage++
      this.loadImageLearnItems(this.imgLearnPage)
      window.scrollTo(0, 0)
    }
  },

  goPage(id) {
    var validPages = this.pages.map(function(p) { return p.id }).concat(['reader'])
    if (validPages.indexOf(id) === -1) {
      id = 'home'
    }
    this.currentPage = id
    if (id === 'clock') {
      this.initClock()
    }
    if (id === 'games') {
      this.stopGame()
    }
  },

  // --- Clock Initialization ---
  async initClock() {
    this.ensureClockLocationFallback()

    // Start the clock loop immediately (uses client-side time)
    this.startClockLoop()
    
    // Fetch all three APIs independently in parallel
    this.fetchClockData()
    this.fetchLocationData()
    this.fetchWeatherData()
  },

  // API 1: Fetch clock time/date from server
  async fetchClockData() {
    try {
      var res = await fetch('/api/clock')
      if (!res.ok) return
      var data = await res.json()
      // Update timezone for analog clock rendering
      if (data.timezone) {
        this.clockTimezone = data.timezone
      }
    } catch (e) {
      console.warn('Clock API failed:', e)
    }
  },

  // API 2: Fetch location message from server
  async fetchLocationData() {
    try {
      var res = await fetch('/api/location')
      if (!res.ok) {
        this.ensureClockLocationFallback()
        return
      }
      var data = await res.json()
      if (data.message) {
        this.clockCity = data.message
        this.locationMessage = data.message
      } else if (data.city) {
        this.clockCity = data.city
        this.locationMessage = data.city
      } else {
        this.ensureClockLocationFallback()
      }
      if (data.timezone) this.clockTimezone = data.timezone
    } catch (e) {
      console.warn('Location API failed:', e)
      this.ensureClockLocationFallback()
    }
  },

  // API 3: Fetch weather from server (auto-detects city from IP)
  async fetchWeatherData() {
    this.weatherLoading = true
    this.weatherError = ''
    try {
      var res = await fetch('/api/weather')
      if (!res.ok) throw new Error('天气获取失败')
      this.weatherData = await res.json()
    } catch (e) {
      if (this.clockCity) {
        await this.fetchClockWeather(this.clockCity)
        return
      }
      this.weatherError = e.message || '天气获取失败'
    } finally {
      this.weatherLoading = false
    }
  },

  ensureClockLocationFallback() {
    var tz = Intl.DateTimeFormat().resolvedOptions().timeZone || 'Asia/Shanghai'
    if (!this.clockTimezone) this.clockTimezone = tz
    if (!this.locationMessage) {
      var parts = tz.split('/')
      var city = parts[parts.length - 1].replace(/_/g, ' ')
      var cityZh = this.translateCity(city)
      this.clockCity = cityZh
      this.locationMessage = cityZh || '当前位置'
    }
  },

  tryGpsLocation() {
    return new Promise(function(resolve) {
      if (!navigator.geolocation) {
        resolve(null)
        return
      }
      navigator.geolocation.getCurrentPosition(
        async function(pos) {
          try {
            // Reverse geocode coordinates to get city name
            var res = await fetch(
              'https://nominatim.openstreetmap.org/reverse?lat=' +
              pos.coords.latitude + '&lon=' + pos.coords.longitude +
              '&format=json&accept-language=zh',
              { headers: { 'User-Agent': 'ebook-app/1.0' } }
            )
            if (!res.ok) { resolve(null); return }
            var data = await res.json()
            var city = data.address &&
              (data.address.city || data.address.town ||
               data.address.county || data.address.state_district ||
               data.address.state)
            if (city) {
              resolve({
                city: city,
                timezone: Intl.DateTimeFormat().resolvedOptions().timeZone
              })
            } else {
              resolve(null)
            }
          } catch(e) {
            resolve(null)
          }
        },
        function() {
          // GPS permission denied or failed
          resolve(null)
        },
        { timeout: 5000, enableHighAccuracy: false }
      )
    })
  },

  async tryIpGeoLocation() {
    try {
      var res = await fetch('/api/geoip')
      if (!res.ok) return null
      var data = await res.json()
      if (data.city && data.timezone) {
        // Translate known English city names to Chinese
        var cityZh = this.translateCity(data.city)
        return { city: cityZh, timezone: data.timezone }
      }
      return null
    } catch (e) {
      return null
    }
  },

  translateCity(en) {
    var map = {
      'Shenzhen': '深圳',
      'Shanghai': '上海',
      'Beijing': '北京',
      'Guangzhou': '广州',
      'Hangzhou': '杭州',
      'Chengdu': '成都',
      'Wuhan': '武汉',
      'Nanjing': '南京',
      'Tianjin': '天津',
      'Chongqing': '重庆',
      'Suzhou': '苏州',
      'Xi\'an': '西安',
      'Changsha': '长沙',
      'Zhengzhou': '郑州',
      'Dongguan': '东莞',
      'Qingdao': '青岛',
      'Shenyang': '沈阳',
      'Ningbo': '宁波',
      'Kunming': '昆明',
      'Dalian': '大连',
      'Xiamen': '厦门',
      'Fuzhou': '福州',
      'Hefei': '合肥',
      'Wuxi': '无锡',
      'Foshan': '佛山',
      'Changzhou': '常州',
      'Jinan': '济南',
      'Harbin': '哈尔滨',
      'Changchun': '长春',
      'Lanzhou': '兰州',
      'Guiyang': '贵阳',
      'Nanning': '南宁',
      'Taiyuan': '太原',
      'Shijiazhuang': '石家庄',
      'Haikou': '海口',
      'Sanya': '三亚',
      'Macau': '澳门',
      'Hong Kong': '香港',
      'Taipei': '台北'
    }
    return map[en] || en
  },

  fallbackClockLocation() {
    // Fallback: extract city from browser timezone
    var tz = Intl.DateTimeFormat().resolvedOptions().timeZone
    this.clockTimezone = tz
    // Try to derive a city name from the timezone (e.g. "Asia/Shanghai" -> "Shanghai")
    var parts = tz.split('/')
    this.clockCity = this.translateCity(parts[parts.length - 1].replace(/_/g, ' '))
    this.locationMessage = this.clockCity
    // Attempt weather fetch with derived city
    this.fetchClockWeather(this.clockCity)
  },

  startClockLoop() {
    if (this._clockAnimFrame) cancelAnimationFrame(this._clockAnimFrame)

    var updateClock = () => {
      this.updateClockDisplay()
      this.drawAnalogClock()
      this._clockAnimFrame = requestAnimationFrame(updateClock)
    }
    updateClock()
  },

  updateClockDisplay() {
    var tz = this.clockTimezone || Intl.DateTimeFormat().resolvedOptions().timeZone
    var now = new Date()
    var parts = new Intl.DateTimeFormat('zh-CN', {
      timeZone: tz,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    }).formatToParts(now)
    var h = parts.find(p => p.type === 'hour').value
    var m = parts.find(p => p.type === 'minute').value
    var s = parts.find(p => p.type === 'second').value
    this.clockTime = h + ':' + m + ':' + s

    this.clockDate = new Intl.DateTimeFormat('zh-CN', {
      timeZone: tz,
      year: 'numeric',
      month: 'long',
      day: 'numeric',
      weekday: 'long'
    }).format(now)
  },

  drawAnalogClock() {
    var container = document.getElementById('clock-canvas-container')
    if (!container) return

    var canvas = container.querySelector('canvas')
    if (!canvas) {
      canvas = document.createElement('canvas')
      canvas.width = 280
      canvas.height = 280
      container.appendChild(canvas)
    }

    var ctx = canvas.getContext('2d')
    var cx = canvas.width / 2
    var cy = canvas.height / 2
    var radius = 125

    var tz = this.clockTimezone || Intl.DateTimeFormat().resolvedOptions().timeZone
    var now = new Date()
    var parts = new Intl.DateTimeFormat('zh-CN', {
      timeZone: tz,
      hour: 'numeric',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    }).formatToParts(now)

    var hour = parseInt(parts.find(p => p.type === 'hour').value)
    var minute = parseInt(parts.find(p => p.type === 'minute').value)
    var second = parseInt(parts.find(p => p.type === 'second').value)

    // Clear
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    // Draw clock face
    ctx.beginPath()
    ctx.arc(cx, cy, radius, 0, Math.PI * 2)
    ctx.fillStyle = '#fff'
    ctx.fill()
    ctx.strokeStyle = '#1a1a2e'
    ctx.lineWidth = 4
    ctx.stroke()

    // Draw hour markers
    for (var i = 0; i < 12; i++) {
      var angle = (i * 30 - 90) * Math.PI / 180
      var innerR = i % 3 === 0 ? radius - 18 : radius - 10
      var outerR = radius - 4
      ctx.beginPath()
      ctx.moveTo(cx + innerR * Math.cos(angle), cy + innerR * Math.sin(angle))
      ctx.lineTo(cx + outerR * Math.cos(angle), cy + outerR * Math.sin(angle))
      ctx.strokeStyle = i % 3 === 0 ? '#1a1a2e' : '#999'
      ctx.lineWidth = i % 3 === 0 ? 3 : 1.5
      ctx.stroke()
    }

    // Draw numbers
    ctx.fillStyle = '#1a1a2e'
    ctx.font = 'bold 14px sans-serif'
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'
    for (var i = 1; i <= 12; i++) {
      var angle = (i * 30 - 90) * Math.PI / 180
      var numR = radius - 28
      ctx.fillText(i.toString(), cx + numR * Math.cos(angle), cy + numR * Math.sin(angle))
    }

    // Draw hour hand
    var hourAngle = ((hour % 12) * 30 + minute * 0.5 - 90) * Math.PI / 180
    ctx.beginPath()
    ctx.moveTo(cx, cy)
    ctx.lineTo(cx + 50 * Math.cos(hourAngle), cy + 50 * Math.sin(hourAngle))
    ctx.strokeStyle = '#1a1a2e'
    ctx.lineWidth = 5
    ctx.lineCap = 'round'
    ctx.stroke()

    // Draw minute hand
    var minAngle = (minute * 6 + second * 0.1 - 90) * Math.PI / 180
    ctx.beginPath()
    ctx.moveTo(cx, cy)
    ctx.lineTo(cx + 75 * Math.cos(minAngle), cy + 75 * Math.sin(minAngle))
    ctx.strokeStyle = '#1a1a2e'
    ctx.lineWidth = 3.5
    ctx.lineCap = 'round'
    ctx.stroke()

    // Draw second hand
    var secAngle = (second * 6 - 90) * Math.PI / 180
    ctx.beginPath()
    ctx.moveTo(cx - 15 * Math.cos(secAngle), cy - 15 * Math.sin(secAngle))
    ctx.lineTo(cx + 85 * Math.cos(secAngle), cy + 85 * Math.sin(secAngle))
    ctx.strokeStyle = '#e63946'
    ctx.lineWidth = 2
    ctx.lineCap = 'round'
    ctx.stroke()

    // Center dot
    ctx.beginPath()
    ctx.arc(cx, cy, 4, 0, Math.PI * 2)
    ctx.fillStyle = '#1a1a2e'
    ctx.fill()
  },

  async fetchClockWeather(city) {
    if (!city) return
    this.weatherLoading = true
    this.weatherError = ''
    try {
      var res = await fetch('/api/weather?city=' + encodeURIComponent(city))
      if (!res.ok) throw new Error('天气获取失败')
      this.weatherData = await res.json()
    } catch (e) {
      this.weatherError = e.message
    } finally {
      this.weatherLoading = false
    }
  },

  // --- Books ---
  async loadBooks(page) {
    page = page || this.libPage
    this.loading = true
    this.error = ''
    try {
      var url = '/api/books?page=' + page + '&perPage=' + this.libPerPage
      var res = await fetch(url)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      var emojiMap = { '科幻': '🚀', '文学': '📝', '童话': '🌟', '历史': '🏛️', '哲学': '💭' }
      this.books = data.items.map(function(b) {
        b.emoji = emojiMap[b.category] || '📖'
        return b
      })
      this.bookTotal = data.total
    } catch (e) {
      this.error = '加载失败: ' + e.message
    } finally {
      this.loading = false
    }
  },

  async loadVoiceStories(page, perPage) {
    page = page || 1
    perPage = perPage || this.voicePerPage
    this.loading = true
    this.error = ''
    try {
      var url = '/api/voice-stories?page=' + page + '&perPage=' + perPage
      var res = await fetch(url)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      this.voiceStories = data.items
      this.voiceTotal = data.total
    } catch (e) {
      this.error = '加载有声书失败: ' + e.message
    } finally {
      this.loading = false
    }
  },

  async loadKidSongs(page, perPage) {
    page = page || 1
    perPage = perPage || this.songPerPage
    this.loading = true
    this.error = ''
    try {
      var url = '/api/kid-songs?page=' + page + '&perPage=' + perPage
      var res = await fetch(url)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      this.kidSongs = data.items
      this.songTotal = data.total
    } catch (e) {
      this.error = '加载儿歌失败: ' + e.message
    } finally {
      this.loading = false
    }
  },

  async loadPoems(page, perPage) {
    page = page || 1
    perPage = perPage || this.poemPerPage
    this.loading = true
    this.error = ''
    try {
      var url = '/api/poems?page=' + page + '&perPage=' + perPage
      var res = await fetch(url)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      this.poems = data.items
      this.poemTotal = data.total
    } catch (e) {
      this.error = '加载古诗失败: ' + e.message
    } finally {
      this.loading = false
    }
  },

  async loadImageLearnItems(page, perPage) {
    page = page || 1
    perPage = perPage || this.imgLearnPerPage
    this.imageLearnLoading = true
    this.imageLearnError = ''
    try {
      var url = '/api/image-learn?page=' + page + '&perPage=' + perPage
      var res = await fetch(url)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      this.imageLearnItems = data.items.map(item => this.withPhotoImage(item))
      this.imgLearnTotal = data.total
    } catch (e) {
      this.imageLearnError = '載入圖片資料失敗，暫用內建資料: ' + e.message
      this.imageLearnItems = []
    } finally {
      this.imageLearnLoading = false
    }
  },

  async loadAllImageLearnItems() {
    this.imageLearnLoading = true
    this.imageLearnError = ''
    try {
      var res = await fetch('/api/image-learn?all=1')
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var data = await res.json()
      this.imageLearnItems = data.items.map(item => this.withPhotoImage(item))
      this.imgLearnTotal = data.total
    } catch (e) {
      this.imageLearnError = '載入圖片資料失敗: ' + e.message
      this.imageLearnItems = []
    } finally {
      this.imageLearnLoading = false
    }
  },

  async playStory(story) {
    this.stopSong()
    this.playingStory = story
    this.speakingStatus = '加载中...'
    
    // Cancel any ongoing speech
    window.speechSynthesis.cancel()

    try {
      var res = await fetch('/api/voice-stories/' + story.id)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var full = await res.json()
      if (!full.content) throw new Error('No content')

      this.playingStory = full
      this.speakingStatus = '准备中...'

      var utterance = new SpeechSynthesisUtterance(full.content)
      utterance.lang = 'zh-CN'
      
      utterance.onstart = function() {
        store.speakingStatus = '播放中...'
      }
      
      utterance.onend = function() {
        store.speakingStatus = '播放完毕'
      }
      
      utterance.onerror = function(e) {
        console.error('TTS Error:', e)
        store.speakingStatus = '播放出错'
      }

      window.speechSynthesis.speak(utterance)
    } catch (e) {
      console.error('Play story error:', e)
      this.speakingStatus = '加载失败'
      this.playingStory = null
    }
  },

  stopStory() {
    window.speechSynthesis.cancel()
    this.playingStory = null
    this.speakingStatus = '停止'
  },

  async playSong(song) {
    this.stopSong()
    this.playingSong = song
    this.songStatus = '加载中...'
    this.playingStory = null
    this.playingPoem = null
    window.speechSynthesis && window.speechSynthesis.cancel()

    try {
      var audio = new Audio(song.audio_url)
      this.songAudio = audio
      audio.onplay = () => { this.songStatus = '播放中...' }
      audio.onended = () => { this.songStatus = '播放完毕'; this.playingSong = null; this.songAudio = null }
      audio.onerror = () => { this.songStatus = '播放出错' }
      await audio.play()
    } catch (e) {
      console.error('Play song error:', e)
      this.songStatus = '播放失败'
    }
  },

  stopSong() {
    if (this.songAudio) {
      try { this.songAudio.pause() } catch(e) {}
      try { this.songAudio.currentTime = 0 } catch(e) {}
    }
    this.songAudio = null
    this.playingSong = null
    this.songStatus = '停止'
  },

  async playPoem(poem) {
    this.stopSong()
    this.playingPoem = poem
    this.playingStory = null
    this.speakingStatus = '加载中...'
    
    window.speechSynthesis.cancel()

    try {
      var res = await fetch('/api/poems/' + poem.id)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var full = await res.json()
      if (!full.content) throw new Error('No content')

      this.playingPoem = full
      this.speakingStatus = '准备中...'

      await this.ensureSpeechVoicesLoaded()

      var utterance = new SpeechSynthesisUtterance(full.content)
      utterance.lang = 'zh-CN'
      var maleVoice = this.selectMaleChineseVoice()
      if (maleVoice) {
        utterance.voice = maleVoice
        // Natural male voice — no pitch hack needed
        utterance.pitch = 1.0
      } else {
        // Fallback: no male voice available, lower pitch slightly to emulate
        utterance.pitch = 0.7
      }
      utterance.rate = 0.5
      
      utterance.onstart = function() {
        store.speakingStatus = '朗讀中...'
      }
      
      utterance.onend = function() {
        store.speakingStatus = '朗讀完毕'
      }
      
      utterance.onerror = function(e) {
        console.error('TTS Error:', e)
        store.speakingStatus = '朗讀出错'
      }

      window.speechSynthesis.speak(utterance)
    } catch (e) {
      console.error('Play poem error:', e)
      this.speakingStatus = '加载失败'
      this.playingPoem = null
    }
  },

  stopPoem() {
    window.speechSynthesis.cancel()
    this.playingPoem = null
    this.speakingStatus = '停止'
  },

  selectMaleChineseVoice() {
    if (!window.speechSynthesis || !window.speechSynthesis.getVoices) return null
    var voices = window.speechSynthesis.getVoices() || []
    if (!voices.length) return null

    var zhVoices = voices.filter(function(voice) {
      return /^zh/i.test(voice.lang || '') || /Chinese|China|Mandarin|Cantonese|Hong Kong|Taiwan|中文|普通话|普通話|粤|粵|國語/i.test(voice.name || '')
    })

    var femaleKeywords = /female|woman|girl|zira|huihui|xiaoxiao|xiaoyi|xiaobei|xiaoni|xiaomo|xiaoqiu|xiaorui|xiaoshuang|xiaoxuan|xiaoyan|xiaozhen|yaoyao|hanhan|meijia|tingting|li-mu|yating|曉|晓|小|女/i
    var maleZhVoices = zhVoices.filter(function(voice) {
      return !femaleKeywords.test(voice.name || '')
    })

    // Known Chinese male voices exposed by Microsoft Edge/Windows and some Chrome setups.
    // Examples: Microsoft Yunxi/Yunyang/Yunjian, Kangkang, Zhiwei, Danny, etc.
    var strongMaleKeywords = /Microsoft.*(Yunxi|Yunyang|Yunjian|Kangkang|Zhiwei|Danny|WanLung)|Yunxi|Yunyang|Yunjian|Kangkang|Zhiwei|Danny|WanLung|男/i
    var preferred = maleZhVoices.find(function(voice) {
      return strongMaleKeywords.test(voice.name || '')
    })
    if (preferred) return preferred

    var maleKeywords = /male|man|david|mark|daniel|alex|yun|kang|liang|zhiwei|zhiyu|wanlung|男/i
    preferred = maleZhVoices.find(function(voice) {
      return maleKeywords.test(voice.name || '')
    })
    if (preferred) return preferred

    // Some browsers do not expose gender in voice names. Prefer non-female Chinese
    // provider voices, otherwise fall back to any Chinese voice.
    var fallbackKeywords = /Microsoft|Google|Sinji|Kangkang|Liang|Zhiwei|Yun/i
    preferred = maleZhVoices.find(function(voice) {
      return fallbackKeywords.test(voice.name || '')
    })
    return preferred || maleZhVoices[0] || zhVoices[0] || null
  },

  ensureSpeechVoicesLoaded() {
    if (!window.speechSynthesis || !window.speechSynthesis.getVoices) return Promise.resolve([])
    var voices = window.speechSynthesis.getVoices()
    if (voices && voices.length) return Promise.resolve(voices)

    return new Promise(function(resolve) {
      var settled = false
      var done = function() {
        if (settled) return
        settled = true
        try { window.speechSynthesis.onvoiceschanged = null } catch(e) {}
        resolve(window.speechSynthesis.getVoices() || [])
      }
      window.speechSynthesis.onvoiceschanged = done
      setTimeout(done, 1200)
    })
  },

  // --- Games ---
  launchGame(gameId) {
    this.currentGame = gameId
    this.playingPoem = null
    this.playingStory = null

    if (gameId === 'wordwrite') {
      this.wmState = 'menu'
      setTimeout(() => {
        this.startWordMatch()
      }, 100)
      return
    }
    
    // Wait for Vue to render the container
    setTimeout(() => {
      const container = document.getElementById('game-canvas-container')
      if (!container) return
      container.innerHTML = ''
      
      const canvas = document.createElement('canvas')
      canvas.width = 400
      canvas.height = 400
      canvas.style.border = '2px solid #333'
      canvas.style.backgroundColor = '#000'
      container.appendChild(canvas)
      
      const ctx = canvas.getContext('2d')
      
      if (gameId === 'racing') {
        this.initRacingGame(ctx, canvas)
      } else if (gameId === 'tetris') {
        this.initTetrisGame(ctx, canvas)
      } else if (gameId === 'shooter') {
        this.initShooterGame(ctx, canvas)
      }
    }, 100)
  },

  gameDisplayName(id) {
    var names = { racing: '瑪利歐賽車', tetris: '俄羅斯方塊', shooter: '太空射擊', wordwrite: '看圖寫字' }
    return names[id] || id
  },

  simKey(key, isDown) {
    var eventType = isDown ? 'keydown' : 'keyup'
    var event = new KeyboardEvent(eventType, { key: key, bubbles: true })
    document.dispatchEvent(event)
  },

  stopGame() {
    this.wmDestroyWriter && this.wmDestroyWriter()
    this.wmState = 'menu'
    this.currentGame = null
    if (this.gameInterval) cancelAnimationFrame(this.gameInterval)
    if (this._enemyInterval) clearInterval(this._enemyInterval)
    if (this._gameKeyDown) window.removeEventListener('keydown', this._gameKeyDown)
    if (this._gameKeyUp) window.removeEventListener('keyup', this._gameKeyUp)
    if (this._clockAnimFrame) cancelAnimationFrame(this._clockAnimFrame)
    if (this._tetrisHandler) window.removeEventListener('keydown', this._tetrisHandler)
        this._gameKeyDown = this._gameKeyUp = this._enemyInterval = this._tetrisHandler = null
  },

  initRacingGame(ctx, canvas) {
    var W = 400, H = 400
    var trackWidth = 200, trackLeft = 100
    var lanes = [trackLeft + 35, trackLeft + 100, trackLeft + 165]
    var playerLane = 1
    var playerY = H - 70
    var carW = 40, carH = 60
    var score = 0
    var speed = 3
    var obstacles = []
    var coins = []
    var frame = 0
    var gameOver = false
    var keys = {}
    var roadOffset = 0

    document.addEventListener('keydown', function(e) { keys[e.key] = true })
    document.addEventListener('keyup', function(e) { keys[e.key] = false })

    var spawnTimer = 0
    var self = this

    function update() {
      if (self.currentGame !== 'racing') {
        document.removeEventListener('keydown', function() {})
        document.removeEventListener('keyup', function() {})
        return
      }

      if (gameOver) {
        ctx.fillStyle = 'rgba(0,0,0,0.8)'
        ctx.fillRect(0, 0, W, H)
        ctx.fillStyle = '#fff'
        ctx.font = 'bold 28px Arial'
        ctx.textAlign = 'center'
        ctx.fillText('💥 撞車了！', W/2, H/2 - 30)
        ctx.font = '18px Arial'
        ctx.fillText('分數: ' + score, W/2, H/2 + 20)
        ctx.fillText('點擊重新開始', W/2, H/2 + 60)
        canvas.onclick = function() {
          canvas.onclick = null
          self.initRacingGame(ctx, canvas)
        }
        return
      }

      // Input
      if (keys['ArrowLeft'] && playerLane > 0) {
        playerLane--
        keys['ArrowLeft'] = false
      }
      if (keys['ArrowRight'] && playerLane < 2) {
        playerLane++
        keys['ArrowRight'] = false
      }
      if (keys['ArrowUp']) speed = Math.min(speed + 0.1, 7)
      if (keys['ArrowDown']) speed = Math.max(speed - 0.2, 2)

      frame++
      roadOffset = (roadOffset + speed) % 80

      // Spawn obstacles
      spawnTimer++
      var spawnInterval = Math.max(20, 60 - Math.floor(speed * 5))
      if (spawnTimer > spawnInterval) {
        spawnTimer = 0
        var lane = Math.floor(Math.random() * 3)
        var obstacleSpeed = 1.5 + Math.random() * speed * 0.5
        obstacles.push({ lane: lane, y: -carH, speed: obstacleSpeed })

        // Sometimes spawn coin
        if (Math.random() < 0.4) {
          var coinLane = Math.floor(Math.random() * 3)
          coins.push({ lane: coinLane, y: -20 })
        }
      }

      score++

      // Draw road
      ctx.clearRect(0, 0, W, H)
      ctx.fillStyle = '#555'
      ctx.fillRect(trackLeft, 0, trackWidth, H)
      ctx.fillStyle = '#eee'
      for (var r = -80 + roadOffset; r < H; r += 80) {
        ctx.fillRect(trackLeft + trackWidth/2 - 3, r, 6, 40)
      }
      ctx.strokeStyle = '#fff'
      ctx.lineWidth = 3
      ctx.strokeRect(trackLeft, 0, trackWidth, H)
      ctx.strokeRect(trackLeft, 0, trackWidth/3, H)
      ctx.strokeRect(trackLeft + trackWidth*2/3, 0, trackWidth/3, H)

      // Move and draw obstacles
      for (var i = obstacles.length - 1; i >= 0; i--) {
        var obs = obstacles[i]
        obs.y += obs.speed

        // Draw car (enemy)
        var ox = lanes[obs.lane] - carW/2
        var colors = ['#e74c3c', '#f39c12', '#9b59b6', '#1abc9c', '#e67e22']
        var c = colors[i % colors.length]
        // Car body
        ctx.fillStyle = c
        ctx.fillRect(ox + 5, obs.y + 8, carW - 10, carH - 8)
        // Roof
        ctx.fillStyle = '#333'
        ctx.fillRect(ox + 10, obs.y + 15, carW - 20, 20)
        // Windows
        ctx.fillStyle = '#87CEEB'
        ctx.fillRect(ox + 12, obs.y + 17, 6, 10)
        ctx.fillRect(ox + carW - 18, obs.y + 17, 6, 10)
        // Wheels
        ctx.fillStyle = '#111'
        ctx.fillRect(ox, obs.y + 10, 8, 12)
        ctx.fillRect(ox + carW - 8, obs.y + 10, 8, 12)
        ctx.fillRect(ox, obs.y + carH - 18, 8, 12)
        ctx.fillRect(ox + carW - 8, obs.y + carH - 18, 8, 12)

        if (obs.y > H) {
          obstacles.splice(i, 1)
        }
      }

      // Draw coins
      for (var i = coins.length - 1; i >= 0; i--) {
        var coin = coins[i]
        coin.y += speed * 0.8
        ctx.fillStyle = '#ffd700'
        ctx.beginPath()
        ctx.arc(lanes[coin.lane], coin.y, 8, 0, Math.PI * 2)
        ctx.fill()
        ctx.fillStyle = '#daa520'
        ctx.font = '10px Arial'
        ctx.textAlign = 'center'
        ctx.fillText('$', lanes[coin.lane], coin.y + 3)

        if (coin.y > H + 10) {
          coins.splice(i, 1)
        }
      }

      // Draw player car (Mario-style)
      var px = lanes[playerLane] - carW/2
      // Body - red like Mario
      ctx.fillStyle = '#e63946'
      ctx.fillRect(px, playerY, carW, carH)
      // Roof/cabin
      ctx.fillStyle = '#c1121f'
      ctx.fillRect(px + 5, playerY + 8, carW - 10, carH - 14)
      // Windshield
      ctx.fillStyle = '#87CEEB'
      ctx.fillRect(px + 10, playerY + 12, carW - 20, 15)
      // M badge (Mario)
      ctx.fillStyle = '#fff'
      ctx.font = 'bold 14px Arial'
      ctx.textAlign = 'center'
      ctx.fillText('M', px + carW/2, playerY + 28)
      // Wheels
      ctx.fillStyle = '#222'
      ctx.fillRect(px - 4, playerY + 8, 8, 14)
      ctx.fillRect(px + carW - 4, playerY + 8, 8, 14)
      ctx.fillRect(px - 4, playerY + carH - 20, 8, 14)
      ctx.fillRect(px + carW - 4, playerY + carH - 20, 8, 14)

      // Collision detection
      var playerRect = { x: px, y: playerY, w: carW, h: carH }
      for (var i = 0; i < obstacles.length; i++) {
        var obs = obstacles[i]
        var ox2 = lanes[obs.lane] - carW/2
        var oRect = { x: ox2 + 4, y: obs.y + 4, w: carW - 8, h: carH - 8 }
        if (playerRect.x < oRect.x + oRect.w &&
            playerRect.x + playerRect.w > oRect.x &&
            playerRect.y < oRect.y + oRect.h &&
            playerRect.y + playerRect.h > oRect.y) {
          gameOver = true
        }
      }

      // Coin collection
      for (var i = coins.length - 1; i >= 0; i--) {
        var coin = coins[i]
        var dx = playerRect.x + playerRect.w/2 - lanes[coin.lane]
        var dy = playerRect.y + playerRect.h/2 - coin.y
        if (Math.abs(dx) < 25 && Math.abs(dy) < 25) {
          coins.splice(i, 1)
          score += 50
        }
      }

      // HUD
      ctx.fillStyle = '#1a1a2e'
      ctx.fillRect(0, 0, W, 38)
      ctx.fillStyle = '#fff'
      ctx.font = 'bold 16px Arial'
      ctx.textAlign = 'left'
      ctx.fillText('🏁 ' + score, 10, 26)
      ctx.textAlign = 'right'
      ctx.fillText('⚡ ' + speed.toFixed(1), W - 10, 26)

      requestAnimationFrame(update)
    }

    update()
  },

  initTetrisGame(ctx, canvas) {
    const ROWS = 20, COLS = 10, BLOCK_SIZE = 20;
    const SHAPES = {
      I: [[1,1,1,1]],
      J: [[1,0,0],[1,1,1]],
      L: [[0,0,1],[1,1,1]],
      O: [[1,1],[1,1]],
      S: [[0,1,1],[1,1,0]],
      T: [[0,1,0],[1,1,1]],
      Z: [[1,1,0],[0,1,1]]
    };
    const COLORS = { I: '#00f0f0', J: '#0000f0', L: '#f0a000', O: '#f0f000', S: '#00f000', T: '#a000f0', Z: '#f00000' };
    
    let board = Array(ROWS).fill().map(() => Array(COLS).fill(0));
    let currentPiece = null, currentPos = {x: 0, y: 0}, currentType = '';
    let score = 0, gameOver = false;

    const spawnPiece = () => {
      const types = Object.keys(SHAPES);
      currentType = types[Math.floor(Math.random() * types.length)];
      currentPiece = SHAPES[currentType];
      currentPos = {x: Math.floor(COLS/2) - 1, y: 0};
      if (collide()) gameOver = true;
    };

    const collide = () => {
      for (let y = 0; y < currentPiece.length; y++) {
        for (let x = 0; x < currentPiece[y].length; x++) {
          if (currentPiece[y][x]) {
            let ny = currentPos.y + y, nx = currentPos.x + x;
            if (ny >= ROWS || nx < 0 || nx >= COLS || (ny >= 0 && board[ny][nx])) return true;
          }
        }
      }
      return false;
    };

    const rotate = (matrix) => {
      return matrix[0].map((_, i) => matrix.map(row => row[i]).reverse());
    };

    const merge = () => {
      currentPiece.forEach((row, y) => {
        row.forEach((val, x) => {
          if (val) board[currentPos.y + y][currentPos.x + x] = currentType;
        });
      });
    };

    const clearLines = () => {
      let lines = 0;
      board = board.filter(row => {
        if (row.every(cell => cell !== 0)) { lines++; return false; }
        return true;
      });
      while (board.length < ROWS) board.unshift(Array(COLS).fill(0));
      score += lines * 100;
    };

    spawnPiece();
    let dropCounter = 0;
    const update = (time = 0) => {
      if (gameOver) {
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(0,0,canvas.width,canvas.height);
        ctx.fillStyle = '#fff';
        ctx.font = '30px Arial';
        ctx.fillText('游戏结束！', 120, 200);
        ctx.font = '20px Arial';
        ctx.fillText('Score: ' + score, 160, 240);
        return;
      }
      
      dropCounter++;
      if (dropCounter > 30) {
        currentPos.y++;
        if (collide()) {
          currentPos.y--;
          merge();
          clearLines();
          spawnPiece();
        }
        dropCounter = 0;
      }

      ctx.fillStyle = '#000';
      ctx.fillRect(0,0,canvas.width,canvas.height);
      
      board.forEach((row, y) => {
        row.forEach((cell, x) => {
          if (cell) {
            ctx.fillStyle = COLORS[cell];
            ctx.fillRect(x*BLOCK_SIZE, y*BLOCK_SIZE, BLOCK_SIZE-1, BLOCK_SIZE-1);
          }
        });
      });

      ctx.fillStyle = COLORS[currentType];
      currentPiece.forEach((row, y) => {
        row.forEach((val, x) => {
          if (val) ctx.fillRect((currentPos.x+x)*BLOCK_SIZE, (currentPos.y+y)*BLOCK_SIZE, BLOCK_SIZE-1, BLOCK_SIZE-1);
        });
      });

      ctx.fillStyle = '#fff';
      ctx.font = '16px Arial';
      ctx.fillText('Score: ' + score, 10, 20);
      
      this.gameInterval = requestAnimationFrame(update);
    };

    this._tetrisHandler = (e) => {
      if (this.currentGame !== 'tetris') return;
      if (e.key === 'ArrowLeft') { currentPos.x--; if (collide()) currentPos.x++; }
      if (e.key === 'ArrowRight') { currentPos.x++; if (collide()) currentPos.x--; }
      if (e.key === 'ArrowDown') { currentPos.y++; if (collide()) currentPos.y--; }
      if (e.key === 'ArrowUp') {
        const prev = currentPiece;
        currentPiece = rotate(currentPiece);
        if (collide()) currentPiece = prev;
      }
    };
    window.addEventListener('keydown', this._tetrisHandler);
    update();
  },

  initShooterGame(ctx, canvas) {
    let player = {x: 200, y: 350, w: 30, h: 30}, bullets = [], enemies = [], score = 0, gameOver = false;
    let stars = Array(50).fill().map(() => ({x: Math.random()*400, y: Math.random()*400, s: Math.random()*2}));
    const keys = {};
    
    this._gameKeyDown = (e) => {
      if (this.currentGame !== 'shooter') return;
      keys[e.key] = true;
    };
    this._gameKeyUp = (e) => {
      if (this.currentGame !== 'shooter') return;
      keys[e.key] = false;
    };
    window.addEventListener('keydown', this._gameKeyDown);
    window.addEventListener('keyup', this._gameKeyUp);

    const spawnEnemy = () => {
      enemies.push({x: Math.random() * 370, y: -30, w: 30, h: 30, speed: 1.5 + Math.random() * 2, type: Math.floor(Math.random()*3)});
    };

    const enemyInterval = setInterval(() => { if (!gameOver) spawnEnemy(); }, 800);

    const drawShip = (x, y, color, isEnemy = false) => {
      ctx.fillStyle = color;
      if (!isEnemy) {
        ctx.beginPath();
        ctx.moveTo(x + 15, y);
        ctx.lineTo(x, y + 30);
        ctx.lineTo(x + 30, y + 30);
        ctx.closePath();
        ctx.fill();
      } else {
        ctx.fillRect(x + 5, y + 5, 20, 20);
        ctx.fillRect(x, y + 10, 5, 10);
        ctx.fillRect(x + 25, y + 10, 5, 10);
      }
    };

    const update = () => {
      if (gameOver) {
        clearInterval(enemyInterval);
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0,0,canvas.width,canvas.height);
        ctx.fillStyle = '#fff';
        ctx.font = 'bold 30px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('MISSION FAILED', 200, 180);
        ctx.font = '20px Arial';
        ctx.fillText('Final Score: ' + score, 200, 220);
        return;
      }

      if (keys['ArrowLeft'] && player.x > 0) player.x -= 6;
      if (keys['ArrowRight'] && player.x < canvas.width - player.w) player.x += 6;
      if (keys['ArrowDown'] && player.y < canvas.height - player.h) player.y += 6;
      if (keys['ArrowUp'] || keys[' ']) {
        bullets.push({x: player.x + 13, y: player.y, w: 4, h: 12});
        keys['ArrowUp'] = false;
        keys[' '] = false;
      }

      stars.forEach(s => {
        s.y += s.s;
        if (s.y > 400) s.y = 0;
      });

      bullets.forEach((b, i) => {
        b.y -= 8;
        if (b.y < 0) bullets.splice(i, 1);
      });

      enemies.forEach((e, i) => {
        e.y += e.speed;
        if (e.y > 400) enemies.splice(i, 1);
        if (e.x < player.x + player.w && e.x + e.w > player.x && e.y < player.y + player.h && e.y + e.h > player.y) gameOver = true;
        bullets.forEach((b, bi) => {
          if (b.x < e.x + e.w && b.x + b.w > e.x && b.y < e.y + e.h && b.y + b.h > e.y) {
            enemies.splice(i, 1);
            bullets.splice(bi, 1);
            score += 10;
          }
        });
      });

      ctx.fillStyle = '#000';
      ctx.fillRect(0,0,canvas.width,canvas.height);
      
      ctx.fillStyle = '#fff';
      stars.forEach(s => ctx.fillRect(s.x, s.y, 1, 1));

      drawShip(player.x, player.y, '#0f0');
      
      ctx.fillStyle = '#ff0';
      bullets.forEach(b => ctx.fillRect(b.x, b.y, b.w, b.h));
      
      enemies.forEach(e => {
        const colors = ['#f00', '#f0f', '#0ff'];
        drawShip(e.x, e.y, colors[e.type], true);
      });

      ctx.fillStyle = '#fff';
      ctx.font = '16px Arial';
      ctx.textAlign = 'left';
      ctx.fillText('Score: ' + score, 10, 20);
      
      this.gameInterval = requestAnimationFrame(update);
    };
    update();
  },

  initPacmanGame(ctx, canvas) {
    const W = 400, H = 400;
    const CELL = 20;
    const COLS = 19, ROWS = 19;

    // Classic Pac-Man maze layout (1=wall, 0=dot, 2=empty, 3=power pellet)
    const baseMaze = [
      [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
      [1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1],
      [1,3,1,1,0,1,1,1,0,1,0,1,1,1,0,1,1,3,1],
      [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1],
      [1,0,1,1,0,1,0,1,1,1,1,1,0,1,0,1,1,0,1],
      [1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1],
      [1,1,1,1,0,1,1,1,0,1,0,1,1,1,0,1,1,1,1],
      [1,1,1,1,0,1,0,0,0,0,0,0,0,1,0,1,1,1,1],
      [1,1,1,1,0,1,0,1,1,2,1,1,0,1,0,1,1,1,1],
      [1,0,0,0,0,0,0,1,2,2,2,1,0,0,0,0,0,0,1],
      [1,1,1,1,0,1,0,1,1,1,1,1,0,1,0,1,1,1,1],
      [1,1,1,1,0,1,0,0,0,0,0,0,0,1,0,1,1,1,1],
      [1,1,1,1,0,1,0,1,1,1,1,1,0,1,0,1,1,1,1],
      [1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1],
      [1,0,1,1,0,1,1,1,0,1,0,1,1,1,0,1,1,0,1],
      [1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1],
      [1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1],
      [1,3,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,3,1],
      [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    ];

    // Deep copy maze
    let maze = baseMaze.map(row => [...row]);
    let score = 0;
    let lives = 3;
    let gameOver = false;
    let gameWin = false;
    let dotsTotal = 0;
    let dotsEaten = 0;

    // Count dots
    for (let r = 0; r < ROWS; r++) {
      for (let c = 0; c < COLS; c++) {
        if (maze[r][c] === 0 || maze[r][c] === 3) dotsTotal++;
      }
    }

    // Pac-Man
    let pac = {
      x: 9, y: 14, // starting position
      dir: {dx: -1, dy: 0},
      nextDir: {dx: -1, dy: 0},
      mouth: 0,
      mouthDir: 1,
      speed: 6 // frames per move
    };

    // Ghosts
    const ghostColors = ['#ff0000', '#ffb8ff', '#00ffde', '#ffb852'];
    let ghosts = [
      {x: 9, y: 8, color: ghostColors[0], dir: {dx: 0, dy: -1}, mode: 'scatter', frightened: false, eaten: false, homeTimer: 0},
      {x: 8, y: 9, color: ghostColors[1], dir: {dx: -1, dy: 0}, mode: 'scatter', frightened: false, eaten: false, homeTimer: 30},
      {x: 10, y: 9, color: ghostColors[2], dir: {dx: 1, dy: 0}, mode: 'scatter', frightened: false, eaten: false, homeTimer: 60},
      {x: 9, y: 10, color: ghostColors[3], dir: {dx: 0, dy: 1}, mode: 'scatter', frightened: false, eaten: false, homeTimer: 90}
    ];

    let frameCount = 0;
    let moveCounter = 0;
    let frightTimer = 0;

    const keyHandler = (e) => {
      if (gameOver || gameWin) return;
      switch (e.key) {
        case 'ArrowLeft': pac.nextDir = {dx: -1, dy: 0}; break;
        case 'ArrowRight': pac.nextDir = {dx: 1, dy: 0}; break;
        case 'ArrowUp': pac.nextDir = {dx: 0, dy: -1}; break;
        case 'ArrowDown': pac.nextDir = {dx: 0, dy: 1}; break;
      }
      e.preventDefault();
    };
    window.addEventListener('keydown', keyHandler);
    this._pacmanHandler = keyHandler;

    function canMove(x, y) {
      if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
      return maze[y][x] !== 1 && maze[y][x] !== 2;
    }

    function isWalkable(x, y) {
      if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
      return maze[y][x] !== 1;
    }

    function wrapX(x) {
      if (x < 0) return COLS - 1;
      if (x >= COLS) return 0;
      return x;
    }

    function ghostTarget(ghost) {
      const corners = [{x: 1, y: 1}, {x: COLS-2, y: 1}, {x: 1, y: ROWS-2}, {x: COLS-2, y: ROWS-2}];
      const scatterTargets = corners;
      const idx = ghosts.indexOf(ghost);
      if (ghost.mode === 'scatter' || ghost.frightened) {
        if (ghost.frightened) {
          return {x: Math.floor(Math.random() * COLS), y: Math.floor(Math.random() * ROWS)};
        }
        return scatterTargets[idx];
      }
      switch (idx) {
        case 0: return {x: pac.x, y: pac.y};
        case 1:
          return {x: pac.x + pac.dir.dx * 4, y: pac.y + pac.dir.dy * 4};
        case 2:
          return {x: pac.x + pac.dir.dx * 2 - ghosts[0].x, y: pac.y + pac.dir.dy * 2 - ghosts[0].y};
        case 3:
          const dist = Math.abs(ghost.x - pac.x) + Math.abs(ghost.y - pac.y);
          if (dist < 8) return scatterTargets[3];
          return {x: pac.x, y: pac.y};
        default: return {x: pac.x, y: pac.y};
      }
    }

    function moveGhost(ghost) {
      if (ghost.homeTimer > 0) {
        ghost.homeTimer--;
        return;
      }

      if (ghost.eaten) {
        const dx = 9 - ghost.x;
        const dy = 9 - ghost.y;
        const moves = [];
        if (dx > 0 && canMove(ghost.x + 1, ghost.y)) moves.push({dx: 1, dy: 0});
        else if (dx < 0 && canMove(ghost.x - 1, ghost.y)) moves.push({dx: -1, dy: 0});
        if (dy > 0 && canMove(ghost.x, ghost.y + 1)) moves.push({dx: 0, dy: 1});
        else if (dy < 0 && canMove(ghost.x, ghost.y - 1)) moves.push({dx: 0, dy: -1});
        if (moves.length > 0) {
          ghost.dir = moves[0];
          ghost.x = wrapX(ghost.x + ghost.dir.dx);
          ghost.y += ghost.dir.dy;
          if (ghost.x === 9 && ghost.y === 9) {
            ghost.eaten = false;
            ghost.mode = 'chase';
          }
        }
        return;
      }

      const target = ghostTarget(ghost);
      const validDirs = [];
      const opposites = [{dx: -1, dy: 0}, {dx: 1, dy: 0}, {dx: 0, dy: -1}, {dx: 0, dy: 1}];

      for (const d of opposites) {
        if (d.dx === -ghost.dir.dx && d.dy === -ghost.dir.dy) continue;
        const nx = wrapX(ghost.x + d.dx);
        const ny = ghost.y + d.dy;
        if (isWalkable(nx, ny)) {
          validDirs.push(d);
        }
      }

      if (validDirs.length === 0) {
        const rev = {dx: -ghost.dir.dx, dy: -ghost.dir.dy};
        ghost.dir = rev;
        ghost.x = wrapX(ghost.x + ghost.dir.dx);
        ghost.y += ghost.dir.dy;
        return;
      }

      let bestDir = validDirs[0];
      let bestDist = Infinity;
      for (const d of validDirs) {
        const nx = wrapX(ghost.x + d.dx);
        const ny = ghost.y + d.dy;
        const dist = Math.abs(nx - target.x) + Math.abs(ny - target.y);
        const speed = ghost.frightened ? 0.5 : 1;
        if (dist * speed < bestDist) {
          bestDist = dist;
          bestDir = d;
        }
      }
      ghost.dir = bestDir;

      if (ghost.frightened && Math.random() < 0.5) {
        ghost.dir = validDirs[Math.floor(Math.random() * validDirs.length)];
      }

      ghost.x = wrapX(ghost.x + ghost.dir.dx);
      ghost.y += ghost.dir.dy;
    }

    function drawMaze() {
      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, W, H);

      for (let r = 0; r < ROWS; r++) {
        for (let c = 0; c < COLS; c++) {
          const x = c * CELL, y = r * CELL;
          if (maze[r][c] === 1) {
            ctx.fillStyle = '#2121de';
            ctx.fillRect(x, y, CELL, CELL);
            ctx.fillStyle = '#0000aa';
            ctx.fillRect(x + 2, y + 2, CELL - 4, CELL - 4);
          } else if (maze[r][c] === 0) {
            ctx.fillStyle = '#ffb8ae';
            ctx.beginPath();
            ctx.arc(x + CELL/2, y + CELL/2, 2, 0, Math.PI * 2);
            ctx.fill();
          } else if (maze[r][c] === 3) {
            ctx.fillStyle = '#ffb8ae';
            ctx.beginPath();
            ctx.arc(x + CELL/2, y + CELL/2, 6, 0, Math.PI * 2);
            ctx.fill();
          }
        }
      }
    }

    function drawPacman() {
      const x = pac.x * CELL + CELL/2;
      const y = pac.y * CELL + CELL/2;
      const r = CELL/2 - 1;
      const mouthAngle = pac.mouth * 0.3;

      ctx.fillStyle = '#ffff00';
      ctx.beginPath();
      let angle = 0;
      if (pac.dir.dx === 1 && pac.dir.dy === 0) angle = 0;
      else if (pac.dir.dx === -1 && pac.dir.dy === 0) angle = Math.PI;
      else if (pac.dir.dx === 0 && pac.dir.dy === -1) angle = -Math.PI/2;
      else if (pac.dir.dx === 0 && pac.dir.dy === 1) angle = Math.PI/2;

      ctx.arc(x, y, r, angle + mouthAngle, angle + 2*Math.PI - mouthAngle);
      ctx.lineTo(x, y);
      ctx.fill();
    }

    function drawGhost(ghost) {
      const x = ghost.x * CELL;
      const y = ghost.y * CELL;
      const r = CELL/2 - 1;

      ctx.fillStyle = ghost.frightened ? '#2121de' : ghost.color;
      ctx.beginPath();
      ctx.arc(x + CELL/2, y + CELL/2 - 2, r, Math.PI, 0);
      ctx.fill();
      for (let i = 0; i < 3; i++) {
        ctx.fillRect(x + 2 + i * 6, y + CELL/2, 6, 6);
      }

      if (!ghost.frightened) {
        ctx.fillStyle = '#fff';
        ctx.beginPath();
        ctx.arc(x + CELL/2 - 4, y + CELL/2 - 3, 4, 0, Math.PI * 2);
        ctx.arc(x + CELL/2 + 4, y + CELL/2 - 3, 4, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#000';
        ctx.beginPath();
        ctx.arc(x + CELL/2 - 4 + ghost.dir.dx * 2, y + CELL/2 - 3 + ghost.dir.dy * 2, 2, 0, Math.PI * 2);
        ctx.arc(x + CELL/2 + 4 + ghost.dir.dx * 2, y + CELL/2 - 3 + ghost.dir.dy * 2, 2, 0, Math.PI * 2);
        ctx.fill();
      } else {
        ctx.fillStyle = '#ffb8ae';
        ctx.beginPath();
        ctx.arc(x + CELL/2 - 4, y + CELL/2 - 3, 3, 0, Math.PI * 2);
        ctx.arc(x + CELL/2 + 4, y + CELL/2 - 3, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    function resetPositions() {
      pac.x = 9; pac.y = 14;
      pac.dir = {dx: -1, dy: 0};
      pac.nextDir = {dx: -1, dy: 0};
      ghosts[0].x = 9; ghosts[0].y = 8; ghosts[0].mode = 'scatter'; ghosts[0].frightened = false; ghosts[0].eaten = false;
      ghosts[1].x = 8; ghosts[1].y = 9; ghosts[1].mode = 'scatter'; ghosts[1].frightened = false; ghosts[1].eaten = false;
      ghosts[2].x = 10; ghosts[2].y = 9; ghosts[2].mode = 'scatter'; ghosts[2].frightened = false; ghosts[2].eaten = false;
      ghosts[3].x = 9; ghosts[3].y = 10; ghosts[3].mode = 'scatter'; ghosts[3].frightened = false; ghosts[3].eaten = false;
    }

    function update() {
      if (store.currentGame !== 'pacman') {
        window.removeEventListener('keydown', keyHandler);
        return;
      }

      if (gameOver) {
        drawMaze();
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0, 0, W, H);
        ctx.fillStyle = '#ff0';
        ctx.font = 'bold 30px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('GAME OVER', W/2, H/2 - 20);
        ctx.fillStyle = '#fff';
        ctx.font = '18px Arial';
        ctx.fillText('Score: ' + score, W/2, H/2 + 20);
        ctx.fillText('點擊重新開始', W/2, H/2 + 60);
        canvas.onclick = () => {
          canvas.onclick = null;
          store.initPacmanGame(ctx, canvas);
        };
        return;
      }

      if (gameWin) {
        drawMaze();
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(0, 0, W, H);
        ctx.fillStyle = '#ff0';
        ctx.font = 'bold 30px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('🎉 YOU WIN! 🎉', W/2, H/2 - 20);
        ctx.fillStyle = '#fff';
        ctx.font = '18px Arial';
        ctx.fillText('Score: ' + score, W/2, H/2 + 20);
        return;
      }

      frameCount++;
      moveCounter++;

      // Pac-Man movement
      if (moveCounter % pac.speed === 0) {
        const tryX = wrapX(pac.x + pac.nextDir.dx);
        const tryY = pac.y + pac.nextDir.dy;
        if (canMove(tryX, tryY)) {
          pac.dir = {dx: pac.nextDir.dx, dy: pac.nextDir.dy};
        }

        const nx = wrapX(pac.x + pac.dir.dx);
        const ny = pac.y + pac.dir.dy;
        if (canMove(nx, ny)) {
          pac.x = nx;
          pac.y = ny;

          if (maze[pac.y][pac.x] === 0) {
            maze[pac.y][pac.x] = 2;
            score += 10;
            dotsEaten++;
          } else if (maze[pac.y][pac.x] === 3) {
            maze[pac.y][pac.x] = 2;
            score += 50;
            dotsEaten++;
            frightTimer = 200;
            ghosts.forEach(g => { g.frightened = true; g.mode = 'frightened'; });
          }

          if (dotsEaten >= dotsTotal) {
            gameWin = true;
          }
        }

        pac.mouth += pac.mouthDir * 0.15;
        if (pac.mouth > 1) { pac.mouth = 1; pac.mouthDir = -1; }
        if (pac.mouth < 0) { pac.mouth = 0; pac.mouthDir = 1; }
      }

      // Ghost movement
      if (moveCounter % 8 === 0) {
        ghosts.forEach(ghost => moveGhost(ghost));
      }

      // Fright timer
      if (frightTimer > 0) {
        frightTimer--;
        if (frightTimer === 0) {
          ghosts.forEach(g => {
            g.frightened = false;
            g.mode = Math.random() < 0.5 ? 'chase' : 'scatter';
          });
        }
      }

      // Mode switching
      if (frameCount % 600 === 0) {
        ghosts.forEach(g => {
          if (!g.frightened && !g.eaten) {
            g.mode = g.mode === 'chase' ? 'scatter' : 'chase';
          }
        });
      }

      // Collision check
      ghosts.forEach(ghost => {
        if (ghost.homeTimer > 0 || ghost.eaten) return;
        if (ghost.x === pac.x && ghost.y === pac.y) {
          if (ghost.frightened) {
            ghost.eaten = true;
            ghost.frightened = false;
            score += 200;
          } else {
            lives--;
            if (lives <= 0) {
              gameOver = true;
            } else {
              resetPositions();
              moveCounter = 0;
            }
          }
        }
      });

      // Draw
      drawMaze();
      ghosts.forEach(ghost => drawGhost(ghost));
      drawPacman();

      // HUD
      ctx.fillStyle = 'rgba(0,0,0,0.8)';
      ctx.fillRect(0, H - 24, W, 24);
      ctx.fillStyle = '#fff';
      ctx.font = '14px Arial';
      ctx.textAlign = 'left';
      ctx.fillText('Score: ' + score, 5, H - 6);
      ctx.textAlign = 'right';
      let livesStr = '';
      for (let i = 0; i < lives; i++) livesStr += '🟡';
      ctx.fillText(livesStr, W - 5, H - 6);

      requestAnimationFrame(update);
    }

    update();
  },

  async openBook(book) {
    this.loading = true
    this.error = ''
    this.currentPage = 'reader'
    window.scrollTo(0, 0)

    // If book already has content (e.g. from a previous full fetch), use it
    if (book.content) {
      this.selectedBook = book
      this.loading = false
      return
    }

    try {
      var res = await fetch('/api/books/' + book.id)
      if (!res.ok) throw new Error('HTTP ' + res.status)
      var full = await res.json()
      this.selectedBook = full
    } catch (e) {
      this.error = '加载书籍失败: ' + e.message
    } finally {
      this.loading = false
    }
  },

  renderContent(text) {
    if (!text) return '<p class="empty">暂无内容</p>'
    return text
      .split('\n')
      .filter(function(l) { return l.trim() })
      .map(function(line) {
        if (/^第.+章/.test(line)) return '<h3 class="chapter">' + line + '</h3>'
        return '<p>' + line + '</p>'
      })
      .join('')
  },

  // --- Word Match (認字遊戲) ---
  get wordMatchData() {
    return this.imageLearnItems
  },

  get imageLearnData() {
    return this.imageLearnItems.map(item => this.withPhotoImage(item))
  },

  withPhotoImage(item) {
    return item // a.pdf image urls are seeded from database, no fallback needed
  },

  photoUrlForWord(item) {
    var word = (item && item.word) || ''
    var query = this.photoQueryForWord(word)
    return 'https://source.unsplash.com/320x240/?' + encodeURIComponent(query + ',real photo,kids learning')
  },

  photoQueryForWord(word) {
    var map = {
      '狗':'dog', '貓':'cat', '鼠':'mouse', '倉鼠':'hamster', '兔':'rabbit', '兔子':'rabbit', '狐':'fox', '熊':'bear', '熊貓':'panda', '樹熊':'koala', '獅':'lion', '獅子':'lion', '虎':'tiger', '老虎':'tiger', '牛':'cow', '豬':'pig', '蛙':'frog', '猴':'monkey', '猴子':'monkey', '雞':'chicken', '企鵝':'penguin', '鳥':'bird', '小雞':'chick', '狼':'wolf', '野豬':'wild boar', '馬':'horse', '獨角獸':'horse toy', '蜜蜂':'bee', '蟲':'insect', '蝴蝶':'butterfly', '蝸牛':'snail', '瓢蟲':'ladybug', '螞蟻':'ant', '蚊子':'mosquito', '蟋蟀':'cricket insect', '龜':'turtle', '烏龜':'turtle', '蜥蜴':'lizard', '蛇':'snake', '章魚':'octopus', '魷魚':'squid', '蝦':'shrimp', '海豚':'dolphin', '鯨':'whale', '鱷魚':'crocodile',
      '蘋果':'apple fruit', '香蕉':'banana', '葡萄':'grapes', '橙':'orange fruit', '橙子':'orange fruit', '檸檬':'lemon', '草莓':'strawberry', '桃':'peach', '桃子':'peach', '櫻桃':'cherry', '菠蘿':'pineapple', '奇異果':'kiwi fruit', '西瓜':'watermelon', '蜜瓜':'melon', '紅蘿蔔':'carrot', '胡蘿蔔':'carrot', '玉米':'corn', '番茄':'tomato', '土豆':'potato', '白菜':'cabbage', '黃瓜':'cucumber', '蘑菇':'mushroom', '南瓜':'pumpkin', '麵包':'bread', '芝士':'cheese', '漢堡':'hamburger', '薯條':'french fries', '熱狗':'hot dog', '薄餅':'pizza', '三文治':'sandwich', '沙律':'salad', '爆谷':'popcorn', '雪糕':'ice cream', '冰淇淋':'ice cream', '冬甩':'donut', '曲奇':'cookies', '餅乾':'cookies', '蛋糕':'cake', '朱古力':'chocolate', '糖果':'candy', '牛奶':'milk', '咖啡':'coffee', '果汁':'juice', '飲料':'drink', '米飯':'rice bowl', '雞蛋':'egg', '水':'water glass',
      '屋':'house', '房子':'house', '學校':'school building', '車':'car', '汽車':'car', '飛機':'airplane', '單車':'bicycle', '自行車':'bicycle', '巴士':'bus', '公交車':'bus', '火車':'train', '帆船':'sailboat', '船':'boat', '火箭':'rocket', '土星':'saturn planet', '太陽':'sun', '月亮':'moon', '星星':'stars', '彩虹':'rainbow', '雲':'cloud', '雨':'rain', '雪':'snow', '火':'fire', '樹':'tree', '花':'flower', '草':'grass', '落葉':'fallen leaves', '樹葉':'leaves', '仙人掌':'cactus', '海浪':'sea waves', '海':'sea ocean', '山':'mountain', '沙灘':'beach',
      '衫':'shirt', '褲':'pants', '帽':'cap', '波鞋':'sneakers', '手套':'gloves', '手袋':'handbag', '眼鏡':'glasses', '嬰兒':'baby', '男孩':'boy', '女孩':'girl', '男人':'man', '女人':'woman', '老人':'elderly person', '婆婆':'grandmother', '爸爸':'father', '媽媽':'mother', '哥哥':'older brother', '姐姐':'older sister', '弟弟':'younger brother', '妹妹':'younger sister', '爺爺':'grandfather', '奶奶':'grandmother',
      '眼睛':'eyes', '耳朵':'ear', '鼻子':'nose', '嘴巴':'mouth', '牙齒':'teeth', '舌頭':'tongue', '頭髮':'hair', '手':'hand', '腳':'foot', '腿':'leg', '肚子':'belly', '床':'bed', '桌子':'table', '椅子':'chair', '書':'book', '鉛筆':'pencil', '書包':'school bag', '球':'ball', '玩具':'toy',
      '紅色':'red color object', '黃色':'yellow color object', '藍色':'blue color object', '綠色':'green color object', '白色':'white color object', '黑色':'black color object', '粉色':'pink color object', '紫色':'purple color object', '上':'up arrow sign', '下':'down arrow sign', '左':'left arrow sign', '右':'right arrow sign', '大':'large object', '小':'small object', '多':'many objects', '少':'few objects'
    }
    return map[word] || word || 'kids learning object'
  },

  imageForWord(item) {
    var word = (item && item.word) || ''
    var emoji = (item && item.emoji) || this.emojiForWord(word)
    var bg = this.colorForWord(word)
    var svg = '<svg xmlns="http://www.w3.org/2000/svg" width="320" height="240" viewBox="0 0 320 240">' +
      '<defs><linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">' +
      '<stop offset="0%" stop-color="' + bg[0] + '"/><stop offset="100%" stop-color="' + bg[1] + '"/>' +
      '</linearGradient></defs>' +
      '<rect width="320" height="240" rx="28" fill="url(#bg)"/>' +
      '<circle cx="268" cy="44" r="34" fill="rgba(255,255,255,.28)"/>' +
      '<circle cx="54" cy="196" r="42" fill="rgba(255,255,255,.18)"/>' +
      '<text x="160" y="132" text-anchor="middle" dominant-baseline="middle" font-size="92" font-family="Apple Color Emoji, Segoe UI Emoji, Noto Color Emoji, sans-serif">' + this.escapeSvg(emoji) + '</text>' +
      '<text x="160" y="210" text-anchor="middle" font-size="34" font-weight="800" fill="#1f2937" font-family="system-ui, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif">' + this.escapeSvg(word) + '</text>' +
      '</svg>'
    return 'data:image/svg+xml;charset=UTF-8,' + encodeURIComponent(svg)
  },

  emojiForWord(word) {
    var map = {
      '爸爸':'👨', '媽媽':'👩', '哥哥':'👦', '姐姐':'👧', '弟弟':'👦', '妹妹':'👧', '爺爺':'👴', '奶奶':'👵', '男孩':'👦', '女孩':'👧',
      '眼睛':'👁️', '耳朵':'👂', '鼻子':'👃', '嘴巴':'👄', '牙齒':'🦷', '頭髮':'💇', '手':'✋', '腳':'🦶', '腿':'🦵', '肚子':'🤰',
      '狗':'🐶', '貓':'🐱', '兔子':'🐰', '兔':'🐰', '熊貓':'🐼', '老虎':'🐯', '虎':'🐯', '獅子':'🦁', '獅':'🦁', '大象':'🐘', '猴子':'🐵', '猴':'🐵', '馬':'🐴', '牛':'🐮', '羊':'🐑', '豬':'🐷', '雞':'🐔', '鴨子':'🦆', '鳥':'🐦', '魚':'🐟', '烏龜':'🐢', '龜':'🐢', '蝴蝶':'🦋', '蜜蜂':'🐝', '螞蟻':'🐜',
      '蘋果':'🍎', '香蕉':'🍌', '葡萄':'🍇', '橙子':'🍊', '橙':'🍊', '草莓':'🍓', '西瓜':'🍉', '桃子':'🍑', '桃':'🍑', '梨':'🍐', '菠蘿':'🍍', '檸檬':'🍋',
      '胡蘿蔔':'🥕', '紅蘿蔔':'🥕', '玉米':'🌽', '番茄':'🍅', '土豆':'🥔', '白菜':'🥬', '黃瓜':'🥒', '蘑菇':'🍄', '南瓜':'🎃',
      '米飯':'🍚', '麵包':'🍞', '雞蛋':'🥚', '牛奶':'🥛', '水':'💧', '蛋糕':'🍰', '餅乾':'🍪', '冰淇淋':'🍦', '雪糕':'🍦',
      '紅色':'🔴', '黃色':'🟡', '藍色':'🔵', '綠色':'🟢', '白色':'⚪', '黑色':'⚫', '粉色':'🌸', '紫色':'🟣',
      '太陽':'☀️', '月亮':'🌙', '星星':'⭐', '雲':'☁️', '雨':'🌧️', '雪':'❄️', '彩虹':'🌈', '花':'🌸', '樹':'🌳', '草':'🌿', '山':'⛰️', '海':'🌊',
      '房子':'🏠', '屋':'🏠', '學校':'🏫', '床':'🛏️', '桌子':'🪑', '椅子':'🪑', '書':'📖', '鉛筆':'✏️', '書包':'🎒', '球':'⚽', '玩具':'🧸',
      '汽車':'🚗', '車':'🚗', '公交車':'🚌', '巴士':'🚌', '火車':'🚆', '飛機':'✈️', '船':'⛵', '自行車':'🚲', '單車':'🚲',
      '上':'⬆️', '下':'⬇️', '左':'⬅️', '右':'➡️', '大':'🔵', '小':'🔹', '多':'🔢', '少':'1️⃣'
    }
    if (map[word]) return map[word]
    var found = this.wordMatchData.find(function(item) { return item.word === word })
    return (found && found.emoji) || '🖼️'
  },

  colorForWord(word) {
    var palettes = [
      ['#fef3c7', '#fca5a5'], ['#dbeafe', '#93c5fd'], ['#dcfce7', '#86efac'],
      ['#fce7f3', '#f9a8d4'], ['#ede9fe', '#c4b5fd'], ['#cffafe', '#67e8f9']
    ]
    var hash = 0
    for (var i = 0; i < (word || '').length; i++) hash = (hash + word.charCodeAt(i)) % palettes.length
    return palettes[hash]
  },

  escapeSvg(value) {
    return String(value || '')
      .replace(/&/g, '&')
      .replace(/</g, '<')
      .replace(/>/g, '>')
      .replace(/"/g, '"')
  },

  // --- Detail Modal ---
  showDetail: null, // the item being viewed in detail, or null
  isRecording: false,
  recordFeedback: '', // 'pass' | 'fail' | ''
  recordingMessage: '', // visible recording status/error text
  recognizedText: '', // the recognized speech text shown next to result
  recognition: null, // SpeechRecognition instance
  recognitionTimer: null, // timeout for auto-stop
  recognitionStartTime: 0, // timestamp when recording started
  recognitionRetryCount: 0, // retry once when the browser reports no speech
  recognitionLanguageIndex: 0, // current hidden STT fallback language index
  bestRecognitionText: '', // best interim/final text captured during this attempt
  bestRecognitionScore: 0, // best similarity score captured during this attempt
  currentRecordingItem: null, // the item being recorded
  speechLanguage: localStorage.getItem('ebookSpeechLanguage') || 'zh-CN',
  speechLanguageOptions: [],
  recordedAudioUrl: '', // blob URL of the recorded audio for playback
  mediaRecorder: null,  // MediaRecorder instance
  audioChunks: [],      // recorded audio chunks
  recordingStream: null,
  audioContext: null,
  audioProcessor: null,
  audioSource: null,
  fallbackAudioBuffers: [],
  fallbackSampleRate: 44100,

  openDetail(item) {
    this.showDetail = item
    this.$nextTick(() => {
      this.initHanziWriter(item.word)
    })
  },

  closeDetail() {
    this.showDetail = null
    // Clean up Hanzi Writer instance if any
    if (this._hanziWriter) {
      try { this._hanziWriter.quit() } catch(e) {}
      this._hanziWriter = null
    }
    // Reset recording state
    this.isRecording = false
    this.recordFeedback = ''
    this.recordingMessage = ''
    this.stopMediaRecorder()
    if (this.recordedAudioUrl) {
      try { URL.revokeObjectURL(this.recordedAudioUrl) } catch(e) {}
      this.recordedAudioUrl = ''
    }
    if (this.recognition) {
      try { this.recognition.abort() } catch(e) {}
      this.recognition = null
    }
    if (this.recognitionTimer) {
      clearTimeout(this.recognitionTimer)
      this.recognitionTimer = null
    }
    this.currentRecordingItem = null
  },

  initHanziWriter(word) {
    // Wait for DOM to render the canvas
    setTimeout(() => {
      var container = document.getElementById('hanzi-writer-canvas')
      if (!container) return
      
      // Clean up any previous instance
      if (this._hanziWriter) {
        try { this._hanziWriter.quit() } catch(e) {}
      }
      this._hanziWriter = null
      
      // Clear container
      container.innerHTML = ''
      container.style.cssText = 'width:200px;height:200px;margin:0 auto;'

      // Load Hanzi Writer via CDN if not loaded
      if (typeof HanziWriter === 'undefined') {
        var script = document.createElement('script')
        script.src = 'https://cdn.jsdelivr.net/npm/hanzi-writer@3.5/dist/hanzi-writer.min.js'
        script.onload = () => {
          this._createHanziWriter(word, container)
        }
        document.head.appendChild(script)
      } else {
        this._createHanziWriter(word, container)
      }
    }, 100)
  },

  _createHanziWriter(word, container) {
    if (typeof HanziWriter === 'undefined') {
      container.innerHTML = '<div style="text-align:center;padding:80px 0;color:#888;">載入筆順中...</div>'
      return
    }
    try {
      var writer = HanziWriter.create(container, word, {
        width: 200,
        height: 200,
        padding: 5,
        showCharacter: false,
        strokeAnimationSpeed: 700,
        delayBetweenStrokes: 300,
        strokeColor: '#333',
        radicalColor: '#e63946',
        outlineColor: '#ddd',
        drawingColor: '#e63946',
        drawingWidth: 8,
        showOutline: true,
        showHintAfterMisses: 3
      })
      this._hanziWriter = writer
      // Animate stroke order, speak word when animation completes
      var self = this
      writer.animateCharacter({
        onComplete: function() {
          self.speakImageWord(self.showDetail)
        }
      })
    } catch (e) {
      console.error('Hanzi Writer error:', e)
      container.innerHTML = '<div style="text-align:center;padding:80px 0;color:#888;">暫無筆順資料</div>'
    }
  },

  handleImageError(item) {
    if (!item || item._fallbackTried) return
    item._fallbackTried = true
    item.image_url = this.imageForWord(item)
  },

  speakImageWord(item) {
    if (!item || !item.word || !window.speechSynthesis) return
    window.speechSynthesis.cancel()
    var utterance = new SpeechSynthesisUtterance(item.word)
    utterance.lang = this.speechLanguage
    utterance.rate = 0.8
    window.speechSynthesis.speak(utterance)
  },

  setSpeechLanguage(lang) {
    this.speechLanguage = lang || 'zh-CN'
    try { localStorage.setItem('ebookSpeechLanguage', this.speechLanguage) } catch(e) {}
    if (this.recognition) {
      try { this.recognition.abort() } catch(e) {}
      this.recognition = null
    }
    this.recordFeedback = ''
    this.recognizedText = ''
    this.recordingMessage = '語音已切換為：' + this.speechLanguageLabel()
  },

  speechLanguageLabel() {
    var current = this.speechLanguage
    var found = this.speechLanguageOptions.find(function(opt) { return opt.value === current })
    return found ? found.label : current
  },

  repeatWord(item) {
    if (!item) return
    // Re-initialize Hanzi Writer to rewrite strokes
    this.initHanziWriter(item.word)
    // Re-pronounce the word
    this.speakImageWord(item)
    // Clear previous recording feedback
    this.recordFeedback = ''
  },

  startRecording(item) {
    if (!item) return
    // Ignore if already recording
    if (this.isRecording) return

    if (!this.canRequestMicrophone()) {
      this.recordFeedback = 'fail'
      this.recordingMessage = '此頁面無法使用麥克風，請用 HTTPS 或 localhost 開啟，並允許麥克風權限。'
      return
    }
    
    window.speechSynthesis && window.speechSynthesis.cancel()
    this.currentRecordingItem = item
    this.recordFeedback = ''
    this.recordingMessage = '正在請求麥克風權限...'
    this.isRecording = true
    this.recognizedText = ''
    this.recognitionRetryCount = 0
    this.recognitionLanguageIndex = 0
    this.bestRecognitionText = ''
    this.bestRecognitionScore = 0
    
    // Start audio recording for playback
    this.audioChunks = []
    if (this.recordedAudioUrl) {
      try { URL.revokeObjectURL(this.recordedAudioUrl) } catch(e) {}
    }
    this.recordedAudioUrl = ''

    this.getMicrophoneStream().then((stream) => {
      if (!this.isRecording) {
        stream.getTracks().forEach(track => track.stop())
        return
      }
      this.recordingStream = stream
      if (typeof MediaRecorder !== 'undefined') {
        this.startMediaRecorder(stream)
      } else {
        this.startWavRecorder(stream)
      }
      this.recordingMessage = '錄音中...請靠近麥克風，清楚讀出文字。'
      this.startSpeechRecognition(item)

      this.recognitionTimer = setTimeout(() => {
        this.stopRecording()
      }, 7000)
    }).catch((err) => {
      console.warn('MediaRecorder error:', err)
      this.isRecording = false
      this.recordFeedback = 'fail'
      this.recordingMessage = '無法使用麥克風，請允許瀏覽器麥克風權限。'
    })
  },

  canRequestMicrophone() {
    return !!(
      (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) ||
      navigator.getUserMedia ||
      navigator.webkitGetUserMedia ||
      navigator.mozGetUserMedia ||
      navigator.msGetUserMedia
    )
  },

  getMicrophoneStream() {
    var constraints = this.getAudioConstraints()
    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
      return navigator.mediaDevices.getUserMedia(constraints)
    }
    var legacyGetUserMedia = navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia || navigator.msGetUserMedia
    return new Promise(function(resolve, reject) {
      if (!legacyGetUserMedia) {
        reject(new Error('getUserMedia unsupported'))
        return
      }
      legacyGetUserMedia.call(navigator, constraints, resolve, reject)
    })
  },

  getAudioConstraints() {
    // Ask the browser/hardware to clean the microphone signal before recording.
    // These constraints are ignored safely by browsers/devices that do not support them.
    return {
      audio: {
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true,
        channelCount: 1,
        sampleRate: 48000,
        sampleSize: 16
      }
    }
  },

  startMediaRecorder(stream) {
    var recorder = new MediaRecorder(stream)
    this.mediaRecorder = recorder
    recorder.ondataavailable = (e) => {
      if (e.data.size > 0) {
        this.audioChunks.push(e.data)
      }
    }
    recorder.onstop = () => {
      this.stopRecordingStream()
      this.mediaRecorder = null
      if (this.audioChunks.length) {
        var blob = new Blob(this.audioChunks, { type: 'audio/webm' })
        this.recordedAudioUrl = URL.createObjectURL(blob)
        this.recordingMessage = '錄音完成，可以按 ▶ 播放。'
      } else {
        this.recordingMessage = '沒有錄到聲音，請再試一次。'
        this.recordFeedback = 'fail'
      }
    }
    recorder.start()
  },

  startWavRecorder(stream) {
    var AudioContext = window.AudioContext || window.webkitAudioContext
    if (!AudioContext) {
      this.isRecording = false
      this.stopRecordingStream()
      this.recordFeedback = 'fail'
      this.recordingMessage = '您的瀏覽器不支援錄音格式，請使用新版 Chrome、Edge 或 Safari。'
      return
    }
    this.fallbackAudioBuffers = []
    this.audioContext = new AudioContext()
    this.fallbackSampleRate = this.audioContext.sampleRate
    this.audioSource = this.audioContext.createMediaStreamSource(stream)
    this.audioProcessor = this.audioContext.createScriptProcessor(4096, 1, 1)
    this.audioProcessor.onaudioprocess = (event) => {
      if (!this.isRecording) return
      var input = event.inputBuffer.getChannelData(0)
      this.fallbackAudioBuffers.push(new Float32Array(input))
    }
    this.audioSource.connect(this.audioProcessor)
    this.audioProcessor.connect(this.audioContext.destination)
  },

  startSpeechRecognition(item) {
    var SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition
    if (!SpeechRecognition) {
      this.recordFeedback = 'fail'
      this.recordingMessage = '此瀏覽器不支援語音識別，請使用 Chrome 或 Edge。'
      return
    }
    
    if (this.recognition) {
      try { this.recognition.abort() } catch(e) {}
      this.recognition = null
    }

    var recognition = new SpeechRecognition()
    recognition.lang = this.getSpeechRecognitionLanguage()
    recognition.continuous = true
    recognition.interimResults = true
    recognition.maxAlternatives = 5
    if (window.SpeechGrammarList || window.webkitSpeechGrammarList) {
      try {
        var SpeechGrammarList = window.SpeechGrammarList || window.webkitSpeechGrammarList
        var grammarList = new SpeechGrammarList()
        var word = String(item.word || '').replace(/[;\\]/g, '')
        grammarList.addFromString('#JSGF V1.0; grammar word; public <word> = ' + word + ' ;', 1)
        recognition.grammars = grammarList
      } catch(e) {}
    }
    
    this.recognition = recognition
    this.recognitionStartTime = Date.now()
    
    recognition.onresult = (event) => {
      // Get the best interim/final result and show it immediately for STT feedback.
      var recognizedText = ''
      var resultIndex = typeof event.resultIndex === 'number' ? event.resultIndex : 0
      var bestScore = -1
      var hasFinal = false
      if (event.results) {
        for (var r = resultIndex; r < event.results.length; r++) {
          var result = event.results[r]
          if (!result) continue
          if (result.isFinal) hasFinal = true
          for (var i = 0; i < result.length; i++) {
            var alt = (result[i].transcript || '').trim()
            if (!alt) continue
            var score = this.scoreRecognitionResult(alt, item)
            // Prefer matching alternatives, then browser confidence, then visible length.
            var confidence = typeof result[i].confidence === 'number' ? result[i].confidence : 0
            var combinedScore = score * 10 + confidence + Math.min(alt.length, 8) / 100
            if (combinedScore > bestScore) {
              bestScore = combinedScore
              recognizedText = alt
            }
            if (score > this.bestRecognitionScore) {
              this.bestRecognitionScore = score
              this.bestRecognitionText = alt
            }
          }
        }
      }
      if (recognizedText) {
        this.recognizedText = recognizedText
        if (!this.bestRecognitionText) this.bestRecognitionText = recognizedText
      }
      if (!recognizedText) return
      if (!hasFinal) return
      // Compare the recognized text to the target word
      var similarity = Math.max(this.scoreRecognitionResult(recognizedText, item), this.bestRecognitionScore)
      this.recordFeedback = similarity >= 0.6 ? 'pass' : 'fail'
    }
    
    recognition.onerror = (event) => {
      if ((event.error === 'no-speech' || event.error === 'language-not-supported') && this.isRecording && this.tryNextRecognitionLanguage(item)) {
        return
      }
      if (event.error === 'no-speech' && this.isRecording && this.recognitionRetryCount < 1) {
        this.recognitionRetryCount++
        this.recordingMessage = '未聽清楚，正在重新識別...請再讀一次。'
        setTimeout(() => {
          if (this.isRecording) this.startSpeechRecognition(item)
        }, 250)
        return
      }
      if (event.error !== 'aborted') {
        this.recordFeedback = 'fail'
        this.recordingMessage = this.speechRecognitionErrorMessage(event.error)
      }
    }
    
    recognition.onend = () => {
      this.recognition = null
      if (this.isRecording && !this.recordFeedback && !this.bestRecognitionText && this.tryNextRecognitionLanguage(item)) {
        return
      }
    }
    
    try {
      recognition.start()
    } catch (e) {
      this.recognition = null
    }
  },

  getSpeechRecognitionLanguage() {
    var languages = this.getSpeechRecognitionLanguages()
    var lang = languages[Math.min(this.recognitionLanguageIndex, languages.length - 1)] || 'zh-CN'
    // Old saved values may include removed Mandarin/Cantonese-specific tags.
    if (['cmn-Hans-CN', 'yue-Hant-HK'].indexOf(lang) !== -1) {
      // Keep these only as hidden fallbacks; do not expose them as UI options.
      return lang
    }
    return lang
  },

  getSpeechRecognitionLanguages() {
    var primary = this.speechLanguage || 'zh-CN'
    var candidates = [primary, 'zh-CN', 'zh-HK', 'zh-TW', 'cmn-Hans-CN', 'yue-Hant-HK']
    var unique = []
    candidates.forEach(function(lang) {
      if (lang && unique.indexOf(lang) === -1) unique.push(lang)
    })
    return unique
  },

  tryNextRecognitionLanguage(item) {
    var languages = this.getSpeechRecognitionLanguages()
    if (this.recognitionLanguageIndex >= languages.length - 1) return false
    this.recognitionLanguageIndex++
    var nextLang = languages[this.recognitionLanguageIndex]
    this.recordingMessage = '正在用另一種中文語音模型重試：' + nextLang
    setTimeout(() => {
      if (this.isRecording && !this.recordFeedback) this.startSpeechRecognition(item)
    }, 250)
    return true
  },

  toggleRecording(item) {
    if (this.isRecording) {
      this.stopRecording()
    } else {
      this.startRecording(item)
    }
  },

  stopRecording() {
    if (this.recognitionTimer) {
      clearTimeout(this.recognitionTimer)
      this.recognitionTimer = null
    }
    // If still recording but no result yet, stop it but don't show feedback
    // (the timeout case — no speech detected)
    if (this.recognition && this.isRecording) {
      try { this.recognition.stop() } catch(e) {}
    }
    this.isRecording = false
    // Stop MediaRecorder if still recording
    this.stopMediaRecorder()
    this.stopWavRecorder()
    // If we reached here without a final result, still show and score the best partial result.
    if (!this.recordFeedback && this.currentRecordingItem) {
      if (this.bestRecognitionText) {
        this.recognizedText = this.bestRecognitionText
        this.recordFeedback = this.bestRecognitionScore >= 0.55 ? 'pass' : 'fail'
      } else {
        this.recordFeedback = 'fail'
      }
    }
  },

  stopMediaRecorder() {
    if (this.mediaRecorder && this.mediaRecorder.state === 'recording') {
      try { this.mediaRecorder.stop() } catch(e) {}
    }
  },

  stopWavRecorder() {
    if (!this.audioProcessor && !this.audioContext) return
    try { if (this.audioProcessor) this.audioProcessor.disconnect() } catch(e) {}
    try { if (this.audioSource) this.audioSource.disconnect() } catch(e) {}
    try { if (this.audioContext && this.audioContext.state !== 'closed') this.audioContext.close() } catch(e) {}
    this.audioProcessor = null
    this.audioSource = null
    this.audioContext = null
    this.stopRecordingStream()
    if (this.fallbackAudioBuffers.length) {
      var wavBlob = this.createWavBlob(this.fallbackAudioBuffers, this.fallbackSampleRate)
      this.recordedAudioUrl = URL.createObjectURL(wavBlob)
      this.recordingMessage = '錄音完成，可以按 ▶ 播放。'
    } else {
      this.recordingMessage = '沒有錄到聲音，請再試一次。'
      this.recordFeedback = 'fail'
    }
  },

  stopRecordingStream() {
    if (this.recordingStream) {
      try { this.recordingStream.getTracks().forEach(track => track.stop()) } catch(e) {}
      this.recordingStream = null
    }
  },

  createWavBlob(buffers, sampleRate) {
    var length = buffers.reduce(function(total, buffer) { return total + buffer.length }, 0)
    var samples = new Float32Array(length)
    var offset = 0
    buffers.forEach(function(buffer) {
      samples.set(buffer, offset)
      offset += buffer.length
    })
    var wavBuffer = new ArrayBuffer(44 + samples.length * 2)
    var view = new DataView(wavBuffer)
    var writeString = function(pos, str) {
      for (var i = 0; i < str.length; i++) view.setUint8(pos + i, str.charCodeAt(i))
    }
    writeString(0, 'RIFF')
    view.setUint32(4, 36 + samples.length * 2, true)
    writeString(8, 'WAVE')
    writeString(12, 'fmt ')
    view.setUint32(16, 16, true)
    view.setUint16(20, 1, true)
    view.setUint16(22, 1, true)
    view.setUint32(24, sampleRate, true)
    view.setUint32(28, sampleRate * 2, true)
    view.setUint16(32, 2, true)
    view.setUint16(34, 16, true)
    writeString(36, 'data')
    view.setUint32(40, samples.length * 2, true)
    var pos = 44
    for (var j = 0; j < samples.length; j++, pos += 2) {
      var s = Math.max(-1, Math.min(1, samples[j]))
      view.setInt16(pos, s < 0 ? s * 0x8000 : s * 0x7fff, true)
    }
    return new Blob([view], { type: 'audio/wav' })
  },

  playRecording() {
    if (!this.recordedAudioUrl) return
    var audio = new Audio(this.recordedAudioUrl)
    audio.play().catch(function(e) {
      console.warn('Audio playback failed:', e)
    })
  },

  speechRecognitionErrorMessage(error) {
    var messages = {
      'no-speech': '沒有聽到語音，請靠近麥克風再試一次。',
      'audio-capture': '無法取得麥克風聲音，請檢查麥克風權限。',
      'not-allowed': '語音識別被瀏覽器封鎖，請允許麥克風/語音權限。',
      'network': '語音識別網路服務連線失敗，請檢查網路後再試。',
      'service-not-allowed': '瀏覽器不允許使用語音識別服務。'
    }
    return messages[error] || ('語音識別失敗：' + error)
  },

  scoreRecognitionResult(recognized, item) {
    if (!item) return 0
    var textScore = this.compareStrings(recognized, item.word)
    var pronunciationScore = this.comparePronunciation(recognized, item)
    return Math.max(textScore, pronunciationScore)
  },

  comparePronunciation(recognized, item) {
    if (!recognized || !item) return 0
    var targetPinyin = this.pinyinForText(item.word, item)
    var recognizedPinyin = this.pinyinForText(recognized, null)
    if (!targetPinyin || !recognizedPinyin) return 0

    var targetSyllables = this.normalizePinyin(targetPinyin).split(' ').filter(Boolean)
    var recognizedSyllables = this.normalizePinyin(recognizedPinyin).split(' ').filter(Boolean)
    if (!targetSyllables.length || !recognizedSyllables.length) return 0

    var targetJoined = targetSyllables.join('')
    var recognizedJoined = recognizedSyllables.join('')
    var editSimilarity = this.stringSimilarity(recognizedJoined, targetJoined)
    var bestSyllableScore = this.bestSyllablePronunciationScore(recognizedSyllables, targetSyllables)

    var syllableMatches = 0
    var len = Math.max(targetSyllables.length, recognizedSyllables.length)
    for (var i = 0; i < Math.min(targetSyllables.length, recognizedSyllables.length); i++) {
      var a = recognizedSyllables[i]
      var b = targetSyllables[i]
      if (a === b) {
        syllableMatches += 1
      } else if (this.stringSimilarity(a, b) >= 0.55 || this.samePinyinInitialFinal(a, b)) {
        syllableMatches += 0.75
      }
    }
    var syllableSimilarity = syllableMatches / Math.max(len, 1)
    var nearHomophoneScore = this.nearHomophoneScore(recognizedSyllables, targetSyllables)
    return Math.max(editSimilarity, syllableSimilarity, nearHomophoneScore, bestSyllableScore)
  },

  bestSyllablePronunciationScore(recognizedSyllables, targetSyllables) {
    // For short child words, browser STT may duplicate the word (蛇蛇) or add
    // extra syllables. If any recognized syllable sounds like any target syllable,
    // treat it as close enough.
    var best = 0
    for (var i = 0; i < recognizedSyllables.length; i++) {
      for (var j = 0; j < targetSyllables.length; j++) {
        best = Math.max(best, this.singleSyllablePronunciationScore(recognizedSyllables[i], targetSyllables[j]))
      }
    }
    return best
  },

  singleSyllablePronunciationScore(aSyllable, bSyllable) {
    if (!aSyllable || !bSyllable) return 0
    if (aSyllable === bSyllable) return 1
    var a = this.splitPinyin(aSyllable)
    var b = this.splitPinyin(bSyllable)
    if (a.final && b.final && a.final === b.final) return 0.9
    if (this.similarPinyinInitial(a.initial, b.initial) && this.stringSimilarity(a.final, b.final) >= 0.4) return 0.8
    if (a.initial === b.initial && this.stringSimilarity(a.final, b.final) >= 0.4) return 0.75
    return this.stringSimilarity(aSyllable, bSyllable)
  },

  nearHomophoneScore(recognizedSyllables, targetSyllables) {
    if (!recognizedSyllables.length || !targetSyllables.length) return 0
    var len = Math.max(recognizedSyllables.length, targetSyllables.length)
    var score = 0
    for (var i = 0; i < Math.min(recognizedSyllables.length, targetSyllables.length); i++) {
      var a = this.splitPinyin(recognizedSyllables[i])
      var b = this.splitPinyin(targetSyllables[i])
      if (recognizedSyllables[i] === targetSyllables[i]) {
        score += 1
      } else if (a.final && b.final && a.final === b.final) {
        // Same final/rhyme is often close enough for young-child pronunciation,
        // e.g. she / zhe / che / se-like recognition errors.
        score += 0.85
      } else if (this.similarPinyinInitial(a.initial, b.initial) && this.stringSimilarity(a.final, b.final) >= 0.45) {
        score += 0.75
      } else if (a.initial === b.initial && this.stringSimilarity(a.final, b.final) >= 0.45) {
        score += 0.7
      } else if (this.stringSimilarity(recognizedSyllables[i], targetSyllables[i]) >= 0.5) {
        score += 0.65
      }
    }
    return score / Math.max(len, 1)
  },

  pinyinForText(text, fallbackItem) {
    var normalizedText = this.normalizeChineseText(text)
    if (!normalizedText) return ''
    if (fallbackItem && fallbackItem.pinyin) return fallbackItem.pinyin

    var exact = this.imageLearnItems.find((entry) => {
      return this.normalizeChineseText(entry.word) === normalizedText && entry.pinyin
    })
    if (exact && exact.pinyin) return exact.pinyin

    // Build a character-level pinyin approximation from existing Image Learn data.
    // This lets STT words that are not exact dataset entries still compare by sound
    // when their characters appear in other learned words.
    var charMap = {}
    this.imageLearnItems.forEach((entry) => {
      var word = this.normalizeChineseText(entry.word)
      var syllables = this.normalizePinyin(entry.pinyin || '').split(' ').filter(Boolean)
      if (word && syllables.length === word.length) {
        for (var i = 0; i < word.length; i++) {
          if (!charMap[word[i]]) charMap[word[i]] = syllables[i]
        }
      }
    })

    var pieces = []
    for (var j = 0; j < normalizedText.length; j++) {
      var py = charMap[normalizedText[j]]
      if (!py) return ''
      pieces.push(py)
    }
    return pieces.join(' ')
  },

  normalizePinyin(value) {
    var toneMap = {
      'ā':'a','á':'a','ǎ':'a','à':'a','ē':'e','é':'e','ě':'e','è':'e',
      'ī':'i','í':'i','ǐ':'i','ì':'i','ō':'o','ó':'o','ǒ':'o','ò':'o',
      'ū':'u','ú':'u','ǔ':'u','ù':'u','ǖ':'v','ǘ':'v','ǚ':'v','ǜ':'v','ü':'v'
    }
    return String(value || '')
      .toLowerCase()
      .replace(/[āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜü]/g, function(ch) { return toneMap[ch] || ch })
      .replace(/[0-5]/g, '')
      .replace(/[^a-zv]+/g, ' ')
      .trim()
  },

  samePinyinInitialFinal(a, b) {
    var pa = this.splitPinyin(a)
    var pb = this.splitPinyin(b)
    if (pa.final && pa.final === pb.final && this.similarPinyinInitial(pa.initial, pb.initial)) return true
    if (pa.initial === pb.initial && this.stringSimilarity(pa.final, pb.final) >= 0.55) return true
    if (pa.final && pb.final && pa.final === pb.final) return true
    return false
  },

  splitPinyin(s) {
    s = String(s || '')
    var initials = ['zh', 'ch', 'sh', 'b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'g', 'k', 'h', 'j', 'q', 'x', 'r', 'z', 'c', 's', 'y', 'w']
    for (var i = 0; i < initials.length; i++) {
      if (s.indexOf(initials[i]) === 0) return { initial: initials[i], final: s.slice(initials[i].length) }
    }
    return { initial: '', final: s }
  },

  similarPinyinInitial(a, b) {
    if (a === b) return true
    var groups = [['zh', 'z', 'j'], ['ch', 'c', 'q'], ['sh', 's', 'x'], ['n', 'l'], ['f', 'h'], ['r', 'l'], ['b', 'p'], ['d', 't'], ['g', 'k']]
    return groups.some(function(group) { return group.indexOf(a) !== -1 && group.indexOf(b) !== -1 })
  },

  stringSimilarity(a, b) {
    a = String(a || '')
    b = String(b || '')
    if (!a || !b) return 0
    if (a === b) return 1
    var distance = this.levenshteinDistance(a, b)
    return 1 - distance / Math.max(a.length, b.length, 1)
  },

  levenshteinDistance(a, b) {
    var dp = []
    for (var i = 0; i <= a.length; i++) {
      dp[i] = [i]
    }
    for (var j = 1; j <= b.length; j++) {
      dp[0][j] = j
    }
    for (var i = 1; i <= a.length; i++) {
      for (var j = 1; j <= b.length; j++) {
        var cost = a[i - 1] === b[j - 1] ? 0 : 1
        dp[i][j] = Math.min(
          dp[i - 1][j] + 1,
          dp[i][j - 1] + 1,
          dp[i - 1][j - 1] + cost
        )
      }
    }
    return dp[a.length][b.length]
  },

  compareStrings(recognized, target) {
    if (!recognized || !target) return 0
    // Normalize: remove spaces/punctuation and handle common Traditional/Simplified variants.
    // Browser STT may return Simplified Chinese even when the learning word is Traditional.
    var clean = (s) => this.normalizeChineseText(s)
    var r = clean(recognized)
    var t = clean(target)
    if (r === t) return 1.0
    
    // Check if target is contained in recognized string (most common for short words)
    if (r.indexOf(t) !== -1) return 1.0
    
    // Character-by-character matching
    var matches = 0
    var tChars = t.split('')
    var rChars = r.split('')
    
    for (var i = 0; i < tChars.length; i++) {
      if (rChars.indexOf(tChars[i]) !== -1) {
        matches++
        // Remove matched char to prevent double-counting
        rChars[rChars.indexOf(tChars[i])] = ''
      }
    }
    
    // Return ratio of matched characters
    return matches / Math.max(t.length, 1)
  },

  normalizeChineseText(value) {
    var s = String(value || '')
      .toLowerCase()
      .replace(/[\s,，。！？、；：""''‘’“”（）、.?!;:()]/g, '')
    var map = {
      '蘋':'苹', '檸':'柠', '櫻':'樱', '蘿':'萝', '蔔':'卜', '麵':'面',
      '餅':'饼', '乾':'干', '漢':'汉', '雞':'鸡', '魚':'鱼', '貓':'猫',
      '狗':'狗', '鳥':'鸟', '馬':'马', '龍':'龙', '龜':'龟', '鴨':'鸭',
      '鵝':'鹅', '豬':'猪', '獅':'狮', '獅':'狮', '象':'象', '車':'车',
      '飛':'飞', '機':'机', '船':'船', '書':'书', '筆':'笔', '門':'门',
      '燈':'灯', '電':'电', '腦':'脑', '話':'话', '鐘':'钟', '錶':'表',
      '傘':'伞', '葉':'叶', '樹':'树', '花':'花', '雲':'云', '風':'风',
      '雨':'雨', '雪':'雪', '陽':'阳', '紅':'红', '綠':'绿', '藍':'蓝',
      '黃':'黄', '黑':'黑', '白':'白', '學':'学', '體':'体', '貝':'贝',
      '寶':'宝', '餃':'饺', '飯':'饭', '麥':'麦', '裏':'里', '裡':'里',
      '個':'个', '這':'这', '說':'说', '聽':'听', '讀':'读', '認':'认', '識':'识',
      '圖':'图', '國':'国', '園':'园', '廣':'广', '東':'东', '萬':'万',
      '長':'长', '發':'发', '頭':'头', '腳':'脚', '體':'体', '齒':'齿',
      '齊':'齐', '對':'对', '雙':'双', '開':'开', '關':'关', '時':'时'
    }
    return s.split('').map(function(ch) { return map[ch] || ch }).join('')
  },

  wmState: 'menu', // 'menu' | 'playing' | 'done'
  wmCurrent: 0,
  wmTotal: 0,
  wmScore: 0,
  wmWrong: 0,
  wmCurrentItem: null,
  wmChoices: [],
  wmCorrectIdx: -1,
  wmAnswered: false,
  wmFeedback: false,
  wmFeedbackType: '',
  wmPool: [],
  // Writing lesson state for wordmatch
  wmRecording: false,
  wmRecognizedText: '',
  wmRecognition: null,
  wmRecognitionTimer: null,
  wmMessage: '',
  wmWriter: null,

  async startWordMatch() {
    this.wmScore = 0
    this.wmWrong = 0
    this.wmCurrent = 0
    this.wmMessage = ''
    // Load all image learn items for the word pool
    this.imageLearnLoading = true
    try {
      var res = await fetch('/api/image-learn?all=1')
      if (res.ok) {
        var data = await res.json()
        this.imageLearnItems = data.items.map(item => this.withPhotoImage(item))
        this.imgLearnTotal = data.total
      }
    } catch(e) {
      // Use whatever items we already have
    } finally {
      this.imageLearnLoading = false
    }
    var pool = this.imageLearnItems.filter(function(item) { return item && item.word })
    this.wmTotal = Math.min(10, pool.length)
    this.wmPool = this.shuffleArray(pool.slice())
    this.wmState = 'playing'
    this.wmShowQuestion()
  },

  wmShowQuestion() {
    if (this.wmCurrent >= this.wmTotal) {
      this.wmState = 'done'
      this.wmDestroyWriter()
      return
    }
    this.wmCurrentItem = this.wmPool[this.wmCurrent]
    this.wmAnswered = false
    this.wmFeedback = false
    this.wmFeedbackType = ''
    this.wmRecognizedText = ''
    this.wmRecording = false
    this.wmMessage = '先看圖片，然後照著灰色字形骨架描寫。'
    this.wmStopRecognition()
    this.$nextTick(() => {
      this.wmInitWriter(false)
    })
  },

  wmLoadHanziWriter() {
    if (typeof HanziWriter !== 'undefined') return Promise.resolve()
    return new Promise(function(resolve, reject) {
      var existing = document.querySelector('script[data-hanzi-writer="1"]')
      if (existing) {
        existing.addEventListener('load', function() { resolve() }, { once: true })
        existing.addEventListener('error', function() { reject(new Error('Hanzi Writer 載入失敗')) }, { once: true })
        return
      }
      var script = document.createElement('script')
      script.src = 'https://cdn.jsdelivr.net/npm/hanzi-writer@3.5/dist/hanzi-writer.min.js'
      script.dataset.hanziWriter = '1'
      script.onload = function() { resolve() }
      script.onerror = function() { reject(new Error('Hanzi Writer 載入失敗')) }
      document.head.appendChild(script)
    })
  },

  wmInitWriter(startQuiz) {
    var item = this.wmCurrentItem
    if (!item || !item.word) return
    var container = document.getElementById('wm-hanzi-writer-canvas')
    if (!container) return
    container.innerHTML = ''
    container.style.cssText = 'width:220px;height:220px;margin:0 auto;'
    this.wmDestroyWriter()
    this.wmLoadHanziWriter().then(() => {
      if (!this.wmCurrentItem || this.wmCurrentItem.word !== item.word) return
      this.wmCreateWriter(item.word, container, startQuiz)
    }).catch((e) => {
      console.warn(e)
      this.wmMessage = '暫時無法載入筆順練習，請檢查網路後再試。'
    })
  },

  wmCreateWriter(word, container, startQuiz) {
    try {
      var self = this
      this.wmWriter = HanziWriter.create(container, word, {
        width: 220,
        height: 220,
        padding: 8,
        showCharacter: false,
        showOutline: true,
        strokeAnimationSpeed: 650,
        delayBetweenStrokes: 250,
        strokeColor: '#333',
        radicalColor: '#e63946',
        outlineColor: '#ddd',
        drawingColor: '#e63946',
        drawingWidth: 10,
        showHintAfterMisses: 2,
        highlightOnComplete: true
      })
      this.wmMessage = '照著灰色字形骨架，一筆一筆描寫。'
      this.wmStartQuiz()
    } catch (e) {
      console.error('WM Hanzi Writer error:', e)
      container.innerHTML = '<div style="text-align:center;padding:80px 0;color:#888;">暫無筆順資料</div>'
      this.wmMessage = '這個字暫時沒有筆順資料。'
    }
  },

  wmStartRecording() {
    // Kept method name for template compatibility; now starts handwriting quiz.
    if (this.wmRecording || this.wmAnswered) return
    this.wmFeedback = false
    this.wmFeedbackType = ''
    this.wmMessage = '請照著灰色骨架，用正確筆順描寫。'
    if (!this.wmWriter) {
      this.wmInitWriter(true)
      return
    }
    this.wmStartQuiz()
  },

  wmStartQuiz() {
    if (!this.wmWriter || this.wmAnswered) return
    this.wmRecording = true
    this.wmMessage = '寫字中：照著骨架，一筆一筆完成。'
    var self = this
    try {
      this.wmWriter.quiz({
        onMistake: function(strokeData) {
          self.wmMessage = '筆順不對，跟著提示再試一次。錯誤 ' + strokeData.mistakesOnStroke + ' 次'
        },
        onCorrectStroke: function(strokeData) {
          self.wmMessage = '很好！第 ' + strokeData.strokeNum + ' 筆寫對了。'
        },
        onComplete: function(summaryData) {
          self.wmRecording = false
          self.wmAnswered = true
          self.wmFeedback = true
          self.wmFeedbackType = 'correct'
          self.wmScore++
          self.wmMessage = '完成！你寫對了「' + self.wmCurrentItem.word + '」。'
          self.wmPronounceWord(self.wmCurrentItem && self.wmCurrentItem.word)
          setTimeout(function() {
            if (self.wmAnswered && self.wmFeedbackType === 'correct') self.wmNext()
          }, 1400)
        }
      })
    } catch (e) {
      console.error('WM quiz error:', e)
      this.wmRecording = false
      this.wmFeedback = true
      this.wmFeedbackType = 'wrong'
      this.wmAnswered = true
      this.wmWrong++
      this.wmMessage = '這個字無法開始筆順測驗，請換下一題。'
    }
  },

  wmStopRecognition() {
    if (this.wmRecognitionTimer) {
      clearTimeout(this.wmRecognitionTimer)
      this.wmRecognitionTimer = null
    }
    if (this.wmRecognition) {
      try { this.wmRecognition.abort() } catch(e) {}
      this.wmRecognition = null
    }
    this.wmRecording = false
  },

  wmDestroyWriter() {
    if (this.wmWriter) {
      try { this.wmWriter.cancelQuiz() } catch(e) {}
      try { this.wmWriter.pauseAnimation() } catch(e) {}
      try { this.wmWriter.hideCharacter() } catch(e) {}
    }
    this.wmWriter = null
  },

  wmPronounceWord(word) {
    if (!word || !window.speechSynthesis) return
    try { window.speechSynthesis.cancel() } catch(e) {}
    var utterance = new SpeechSynthesisUtterance(word)
    utterance.lang = 'zh-CN'
    utterance.rate = 0.85
    try { window.speechSynthesis.speak(utterance) } catch(e) {}
  },

  wmReplayWord() {
    // Keep for compatibility, but simply refresh the current outline.
    if (!this.wmCurrentItem || !this.wmCurrentItem.word) return
    this.wmRecording = false
    this.wmFeedback = false
    this.wmFeedbackType = ''
    this.wmMessage = '重新顯示字形骨架，請再描寫一次。'
    this.wmInitWriter(false)
  },

  wmRetry() {
    // Kept for compatibility; refresh the current skeleton.
    this.wmAnswered = false
    this.wmFeedback = false
    this.wmFeedbackType = ''
    this.wmRecognizedText = ''
    this.wmRecording = false
    this.wmStopRecognition()
    this.wmInitWriter(true)
  },

  wmAnswer(idx) {
    // Legacy - kept for compatibility, not used in writing UI
  },

  wmNext() {
    this.wmDestroyWriter()
    this.wmCurrent++
    this.wmShowQuestion()
  },

  wmChoiceClass(idx) {
    return ''
  },

  wmBuildChoices() {
    // No longer needed - writing based, but keep empty to avoid errors
  },

  get wmStars() {
    var pct = this.wmScore / Math.max(this.wmTotal, 1)
    if (pct >= 0.9) return 3
    if (pct >= 0.6) return 2
    if (pct >= 0.3) return 1
    return 0
  },

  get wmResultMsg() {
    var stars = this.wmStars
    if (stars === 3) return '太棒了！筆順都寫對了！🎉'
    if (stars === 2) return '不錯哦！繼續練習寫字！💪'
    if (stars === 1) return '還需努力，多寫幾次就會了！📚'
    return '多加練習吧！你行的！💯'
  },
  shuffleArray(arr) {
    var a = arr.slice()
    for (var i = a.length - 1; i > 0; i--) {
      var j = Math.floor(Math.random() * (i + 1));
      var tmp = a[i]
      a[i] = a[j]
      a[j] = tmp
    }
    return a
  }
}

PetiteVue.createApp(store).mount('#app')
store.loadBooks()
store.loadVoiceStories()
store.loadKidSongs()
store.loadPoems()
store.loadImageLearnItems()