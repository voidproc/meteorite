# include <Siv3D.hpp> // OpenSiv3D v0.6.12

// 隕石がダメージを受けて消滅する時のはじけるエフェクト
struct ExplodeEffect : IEffect
{
	ExplodeEffect(const Vec2& pos)
		:
		pos_{ pos },
		distance_{ Random(8.0, 48.0), Random(360_deg) },
		size_{ Random(4.0, 12.0) },
		timerLifetime_{ SecondsF{Random(0.15, 0.4)}, StartImmediately::Yes }
	{
	}

	bool update(double t) override
	{
		const Vec2 pos = pos_ + distance_.fastToVec2() * EaseOutCubic(timerLifetime_.progress0_1());

		RectF{ Arg::center = pos, size_ }
			.rotated(Scene::Time() * size_)
			.draw(ColorF{ Palette::Yellow, timerLifetime_.progress1_0() })
			.drawFrame(4.0 * timerLifetime_.progress1_0(), 0.0, Palette::Yellow);

		return not timerLifetime_.reachedZero();
	}

	Vec2 pos_;
	Circular distance_;
	double size_;
	Timer timerLifetime_;
};

void DrawRatio(const Vec2& pos, double ratio, Optional<double> sizeFixed = none)
{
	ColorF color;
	double size;

	if (ratio < 4.0)
	{
		color = Palette::Lime.lerp(Palette::Green, Periodic::Jump0_1(0.2s));
		size = 14;
	}
	else if (ratio < 7.0)
	{
		color = Palette::Orange.lerp(Palette::Saddlebrown, Periodic::Jump0_1(0.2s));
		size = 18;
	}
	else
	{
		color = Palette::Orangered.lerp(Palette::Yellow, Periodic::Jump0_1(0.2s));
		size = 22;
	}

	FontAsset(U"Score")(U"x{:.1f}"_fmt(ratio)).drawAt(sizeFixed ? *sizeFixed : size, pos, color);
}

// 隕石破壊時の倍率表示
struct RatioEffect : IEffect
{
	RatioEffect(const Vec2& pos, double score, double ratio)
		: pos_{ pos }, score_{ score }, ratio_{ ratio }
	{
	}

	bool update(double t) override
	{
		DrawRatio(pos_.movedBy(EaseOutCubic(t) * 16.0, 0), ratio_);

		return t < 0.5;
	}

	Vec2 pos_;
	double score_;
	double ratio_;
};

// 星
struct StarEffect : IEffect
{
	StarEffect()
		: pos_{ Scene::Width() + Random(0, 16), Random(16, Scene::Height() - 16) }, speed_{ Random(0.5, 8.0) }
	{
	}

	StarEffect(double x)
		: pos_{ x, Random(16, Scene::Height() - 16) }, speed_{ Random(0.5, 8.0) }
	{

	}

	bool update(double t) override
	{
		pos_.x -= speed_ * 60 * Scene::DeltaTime();

		Shape2D::NStar(4, Random(2, 8), 1, pos_, 0_deg).draw(ColorF{ Palette::Cyan.lerp(Palette::Pink, Periodic::Jump0_1(0.08s)), Random(0.1, 0.85) });

		return pos_.x > -16;
	}

	Vec2 pos_;
	double speed_;
};

// 隕石
// いろんな大きさの隕石が画面右から押し寄せてくる
// 画面外に出るかライフがなくなると消える

enum class MeteoriteType
{
	Destroyable,
	Return,
};

class Meteorite
{
public:
	Meteorite(const Vec2& pos, const Circular& velocity, double size, MeteoriteType type = MeteoriteType::Destroyable)
		:
		pos_{ pos },
		velocity_{ velocity },
		size_{ size },
		time_{ StartImmediately::Yes },
		rotationSpeed_{ (RandomBool() ? 1 : -1) * Random(1.2, 2.5) },
		collision_{ pos, 1 },
		life_{ size },
		timerDamaged_{ 0.2s },
		damaged_{ false },
		type_{ type }
	{
	}

	void update()
	{
		if (type_ == MeteoriteType::Return)
		{
			// 加速
			velocity_.r += 2.5 * Scene::DeltaTime();
		}

		pos_ += velocity_.fastToVec2() * Scene::DeltaTime() * 60.0;

		collision_.set(pos_, size_);
	}

	void draw() const
	{
		const double tDamageFx = Periodic::Square0_1(0.1s / 8);
		const Color baseColor = type_ == MeteoriteType::Destroyable ? Palette::White : Palette::Orange.lerp(Palette::Magenta, Periodic::Jump0_1(0.3s));
		const ColorF color = timerDamaged_.isRunning() ? ColorF{ 1.0 - 0.2 * tDamageFx, 1.0 * tDamageFx, 1.0 * tDamageFx } : ColorF{ baseColor };

		TextureAsset(U"Meteorite").resized(size_ * 2.5).rotated(time_.sF() * rotationSpeed_).drawAt(pos_, color);
	}

	bool isAlive() const
	{
		if (life_ < 0) return false;

		if (time_ < 1s) return true;

		return collision().intersects(Scene::Rect().stretched(32));
	}

	const Circle& collision() const
	{
		return collision_;
	}

	void damage(double damageAmount)
	{
		if (type_ != MeteoriteType::Destroyable) return;

		life_ -= damageAmount;

		if (not timerDamaged_.isRunning())
		{
			timerDamaged_.restart(0.2s);
		}

		// 減速
		if (not damaged_)
		{
			damaged_ = true;
			velocity_.r *= 0.2;
		}
	}

	double life() const
	{
		return life_;
	}

	double size() const
	{
		return size_;
	}

private:
	// 現在の位置
	Vec2 pos_;

	// 現在の速度
	Circular velocity_;

	// 大きさ
	double size_;

	// 経過時間
	Stopwatch time_;

	// 回転速度
	double rotationSpeed_;

	// 当たり判定
	Circle collision_;

	// 耐久力
	double life_;

	// ダメージを受けた
	Timer timerDamaged_;
	bool damaged_;

	// 種類
	MeteoriteType type_;

};

using MeteoritePtr = std::shared_ptr<Meteorite>;

class Player
{
public:
	Player()
		: pos_{ 32, Scene::Height() / 2 }
	{
	}

	void update()
	{
		if (life_ <= 0) return;

		const double MaxSpeed = 12.0 * 60 * Scene::DeltaTime();
		Vec2 posDiff = Cursor::PosF() - pos_;
		if (posDiff.length() > MaxSpeed) posDiff.setLength(MaxSpeed);

		pos_ += posDiff;
		pos_.x = Clamp(pos_.x, 24.0, Scene::Width() - 24.0);
		pos_.y = Clamp(pos_.y, 24.0, Scene::Height() - 24.0);

		collision_.set(pos_, 16);
	}

	void draw(bool insideBarrier) const
	{
		Vec2 pos = pos_;

		if (life_ <= 0)
		{
			pos += RandomVec2(16.0);
		}

		TextureAsset(U"Player").resized(50).drawAt(pos, insideBarrier ? Color{ 100, 255, 100 } : Palette::White);
		Circle{ pos, 64 * Periodic::Sawtooth0_1(0.7s) }.drawFrame(1.0, 0.0, ColorF{ Palette::Lime, 0.8 - 0.8 * Periodic::Sawtooth0_1(0.7s) });
	}

	const Vec2& pos() const
	{
		return pos_;
	}

	int life() const
	{
		return life_;
	}

	void damage()
	{
		--life_;
	}

	const Circle& collision() const
	{
		return collision_;
	}

private:
	Vec2 pos_;
	int life_ = 1;
	Circle collision_;
};

class Barrier
{
public:
	Barrier(const Vec2& pos, const Circular& velocity, double size)
		:
		pos_{ pos },
		velocity_{ velocity },
		size_{ size },
		time_{ StartImmediately::Yes },
		timeActivated_{},
		collision_{ pos, 1 }
	{
	}

	void update(const Player& player)
	{
		const auto vel = velocity_.fastToVec2() * (1.0 - 0.85 * EaseOutCubic(Clamp(time_.sF() / 0.8, 0.0, 1.0)));
		pos_ += vel * Scene::DeltaTime() * 60.0;

		isPlayerInside_ = player.pos().intersects(collision());

		if (not isActivated() && isPlayerInside())
		{
			timeActivated_.start();

			velocity_.r *= 0.3;
		}

		// 範囲内に入ると広がる
		double size = size_;
		if (isActivated())
		{
			const double t = Clamp(timeActivated_.sF() / 0.2, 0.0, 1.0);
			size += size_ * 0.7 * EaseOutCubic(t);
		}

		collision_.set(pos_, size);
	}

	void draw() const
	{
		if (isActivated())
		{
			const double alpha = timeActivated_ > 2s ? 0.8 * Periodic::Jump0_1(0.15s) : 1.0;

			collision()
				.draw(ColorF{ Palette::Lime, 0.08 + 0.04 * Periodic::Sine0_1(0.1s) * alpha })
				.drawFrame(2.0, 0.0, ColorF{ Palette::Lime, alpha });

			collision().scaled(EaseOutCubic(Periodic::Sawtooth0_1(0.4s))).drawFrame(1.0, 0.0, ColorF{ Palette::Lime, 0.5 * (1.0 - Periodic::Sawtooth0_1(0.4s)) });

		}
		else
		{
			collision().drawFrame(2.0, 0.0, ColorF{ Palette::Lime, 0.5 });
			FontAsset(U"BarrierState")(U"NOT ACTIVATED").drawAt(12, pos_, ColorF{ Palette::Lime, 0.8 });
		}
	}

	bool isAlive() const
	{
		if (time_ < 1s) return true;

		if (timeActivated_ > 3.5s) return false;

		return collision().intersects(Scene::Rect().stretched(32));
	}

	bool isActivated() const
	{
		return timeActivated_.isRunning();
	}

	const Circle& collision() const
	{
		return collision_;
	}

	bool isPlayerInside() const
	{
		return isPlayerInside_;
	}

private:
	// 現在の位置
	Vec2 pos_;

	// 現在の速度
	Circular velocity_;

	// 大きさ
	double size_;

	// 経過時間
	Stopwatch time_;

	// アクティベートされてからの経過時間
	Stopwatch timeActivated_;

	// 当たり判定
	Circle collision_;

	// プレイヤーがいる
	bool isPlayerInside_ = false;

};

using BarrierPtr = std::shared_ptr<Barrier>;


void UpdateMeteorites(Array<MeteoritePtr>& meteorites, Player& player, Effect& effect)
{
	for (const auto& m : meteorites)
	{
		m->update();

		if (player.life() > 0 && m->collision().intersects(player.collision().center))
		{
			player.damage();

			m->damage(9999 * Scene::DeltaTime());

			for (int i : step(8))
				effect.add<ExplodeEffect>(m->collision().center);
		}
	}

	meteorites.remove_if([](const auto& m) { return not m->isAlive(); });
}

void DrawMeteorites(Array<MeteoritePtr>& meteorites)
{
	for (const auto& m : meteorites)
	{
		m->draw();
	}
}

void UpdateBarriers(Array<BarrierPtr>& barriers, const Player& player, Array<MeteoritePtr>& meteorites, Effect& effect, double rank, double ratio, int& score)
{
	for (const auto& b : barriers)
	{
		b->update(player);
	}

	barriers.remove_if([](const auto& b) { return not b->isAlive(); });


	Array<Vec2> returnBulletsPos;

	for (const auto& b : barriers)
	{
		if (b->isActivated())
		{
			for (const auto& m : meteorites)
			{
				if (b->collision().intersects(m->collision()))
				{
					if (m->life() >= 0)
					{
						m->damage(60 * Scene::DeltaTime());

						if (m->life() <= 0)
						{
							score += (((int)(100 + 10 * m->size() * 5) * ((int)(ratio * 10))) / 100) * 10;

							if (RandomBool(0.30 * rank))
								returnBulletsPos << m->collision().center;

							for (int i : step(8))
								effect.add<ExplodeEffect>(m->collision().center);

							effect.add<RatioEffect>(m->collision().center, 0, ratio);
						}
					}
				}
			}
		}
	}

	// 撃ち返し
	for (const auto& p : returnBulletsPos)
	{
		// 避け辛い撃ち返しの生成を避ける
		if (p.x < player.pos().x) continue;
		if ((p - player.pos()).length() < 60) continue;

		{
			const Vec2 pos = p + RandomVec2(8.0);
			const Circular vel{ Random(-1.0, 0.0), Atan2(player.pos().x - p.x, p.y - player.pos().y) + Random(-5_deg, 5_deg) };
			meteorites.emplace_back(std::make_unique<Meteorite>(pos, vel, 10.0, MeteoriteType::Return));
		}
	}
}

void DrawBarriers(Array<BarrierPtr>& barriers)
{
	for (const auto& b : barriers)
	{
		b->draw();
	}
}

bool IsPlayerInsideBarriers(Array<BarrierPtr>& barriers)
{
	for (const auto& b : barriers)
	{
		if (b->isPlayerInside())
		{
			return true;
		}
	}
	return false;
}

void DrawScore(int score, double ratio)
{
	FontAsset(U"Score")(U"{:08d}"_fmt(score)).drawAt(TextStyle::Outline(0.2, Palette::Black), 32, Scene::Rect().topCenter().movedBy(0, 32));

	DrawRatio(Vec2{ 48, Scene::Rect().y + 32 }, ratio, 32);
}

void DrawBG(const Texture& bgTexture)
{
	bgTexture((int)(Scene::Time() * 64) % 1024, 0, 256, 256).resized(Scene::Size()).draw(ColorF{ 1, 0.17 });
	Scene::Rect().stretched(0, -220).movedBy(0, -220).draw(Arg::top = ColorF{ Palette::Blue, 0.2 }, Arg::bottom = ColorF{ Palette::Blue, 0.0 });
	Scene::Rect().stretched(0, -220).movedBy(0, +220).draw(Arg::top = ColorF{ Palette::Blue, 0.0 }, Arg::bottom = ColorF{ Palette::Blue, 0.2 });
}

void Main()
{
	Window::SetTitle(U"Meteorite Protection System v1.0.0");

	Scene::SetBackground(ColorF{ 0 });

	TextureAsset::Register(U"Meteorite", U"⭐"_emoji);
	TextureAsset::Register(U"ReturnBullet", U"💠"_emoji);
	TextureAsset::Register(U"Player", U"🛰️"_emoji);
	FontAsset::Register(U"BarrierState", 16, Typeface::Thin, FontStyle::Bold);
	FontAsset::Register(U"Score", FontMethod::MSDF, 24, Typeface::Bold);

	Array<MeteoritePtr> meteorites;
	Array<BarrierPtr> barriers;
	Player player;
	Stopwatch timeTitle{ StartImmediately::Yes };
	Stopwatch time;
	Timer timerPlayerdead{ 2s };
	Timer timerSpawnMeteorites{ 0.2s, StartImmediately::Yes };
	Timer timerSpawnBarriers{ 0.8s, StartImmediately::Yes };
	Timer timerSpawnStars{ 0.05s, StartImmediately::Yes };
	Effect effect, starEffect;
	double ratio = 1.0;
	int score = 0;
	int hiscore = 0;

	// もやもや画像
#if SIV3D_PLATFORM(WINDOWS)
	Image bgImage{ Resource(U"assets/moyamoya.png") };
#elif SIV3D_PLATFORM(WEB)
	Image bgImage{ U"assets/moyamoya.png" };
#endif
	bgImage
		.blur(4)
		.grayscale();
	Texture bgTexture{ bgImage };

	// 星
	for (int i : step(32))
	{
		starEffect.add<StarEffect>(Random(0, Scene::Width()));
	}

	while (System::Update())
	{
		// 背景

		if (timerSpawnStars.reachedZero())
		{
			timerSpawnStars.restart();

			if (starEffect.num_effects() < 150)
				starEffect.add<StarEffect>();
		}

		starEffect.update();

		DrawBG(bgTexture);

		// タイトル
		if (timeTitle.isRunning())
		{
			if (MouseL.down())
			{
				timeTitle.reset();
				time.restart();
			}

			FontAsset(U"BarrierState")(U"HISCORE").drawAt(16, Scene::CenterF().movedBy(0, -64));
			FontAsset(U"Score")(U"{:08d}"_fmt(hiscore)).drawAt(TextStyle::Outline(0.2, Palette::Black), 48, Scene::CenterF().movedBy(0, -24));
			FontAsset(U"BarrierState")(U"CLICK TO START MISSION").drawAt(18, Scene::CenterF().movedBy(0, 112));
		}

		// メイン
		if (time.isRunning())
		{
			// ランク（難易度）
			const double rank = EaseInOutSine(Clamp(time.sF() / 120.0, 0.0, 1.0));

			const bool isPlayerInsideBarriers = IsPlayerInsideBarriers(barriers);

			// スコア倍率
			// バリア外かつ画面右に行くほど倍率が高まる
			const double ratioBonus = isPlayerInsideBarriers ? 0 : EaseInOutSine(Clamp((player.pos().x - Scene::Width() * 0.2) / Scene::Width() * 0.8, 0.0, 1.0));
			ratio += ratioBonus * Scene::DeltaTime();
			if (ratioBonus <= 1e-3)
			{
				ratio -= 0.1 * Scene::DeltaTime();
			}
			ratio = Clamp(ratio, 1.0, 8.0);

			// 隕石が右から押し寄せてくる
			if (timerSpawnMeteorites.reachedZero())
			{
				timerSpawnMeteorites.restart();

				for (int i : step(Random<int>(1 + 2 * rank, 6 + 4 * rank)))
				{
					const Vec2 pos = RandomVec2(RectF{ Scene::Width(), 0, 16, Scene::Height() }.stretched(0, -16));
					const Circular vel{ Random(0.75 - 0.3 * rank, 3.0 + 5.0 * rank), 270_deg + Random(-20_deg, 20_deg) };
					const double size = 16.0 + Random(-8.0, 8.0 + 10.0 * rank); //8～34
					meteorites.emplace_back(std::make_shared<Meteorite>(pos, vel, size));
				}
			}

			// バリアが左から送出される
			if (timerSpawnBarriers.reachedZero())
			{
				timerSpawnBarriers.restart(SecondsF{ Random(0.75 - 0.2 * rank, 1.5 - 0.3 * rank) });

				const Vec2 pos = RandomVec2(RectF{ 0, 0, 16, Scene::Height() }.stretched(0, -48));
				const Circular vel{ Random(5.0, 15.0), 90_deg + Sign(Scene::Height() / 2.0 - pos.y) * Random(0_deg, 20_deg) };
				const double size = 20.0 + Random(0.0, 60.0);
				barriers.emplace_back(std::make_shared<Barrier>(pos, vel, size));
			}


			UpdateBarriers(barriers, player, meteorites, effect, rank, ratio, score);
			DrawBarriers(barriers);

			UpdateMeteorites(meteorites, player, effect);

			if (player.life() <= 0 && timerPlayerdead.reachedZero())
			{
				// ゲームをリセット
				meteorites.clear();
				barriers.clear();
				effect.clear();
				player = Player{};
				if (hiscore < score)
				{
					hiscore = score;
				}
				score = 0;

				timerPlayerdead.reset();
				time.reset();
				timeTitle.restart();
				continue;
			}

			if (player.life() <= 0 && not timerPlayerdead.isRunning())
			{
				// ゲームオーバー
				timerPlayerdead.restart(2s);
			}

			DrawMeteorites(meteorites);

			effect.update();

			player.update();
			player.draw(isPlayerInsideBarriers);

			if (timerPlayerdead.isRunning())
			{
				const double alpha = EaseOutCubic(Clamp((timerPlayerdead.sF() - 1.5) / 0.5, 0.0, 1.0));
				Scene::Rect().draw(ColorF{ Palette::Red, alpha * 0.4 });
			}

			if (time < 0.3s)
			{
				const double alpha = EaseOutCubic(Clamp((0.3 - time.sF()) / 0.3, 0.0, 1.0));
				Scene::Rect().draw(ColorF{ 0, alpha });
			}

			DrawScore(score, ratio);
		}
	}
}
